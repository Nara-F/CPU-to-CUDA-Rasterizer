#include <cuda_runtime.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "cuda_rasterizer.hpp"
#include "raster_math.hpp"
#include "render_entry.hpp"

namespace minigpu {

namespace {

void check_cuda(cudaError_t status, const char* operation) {
    if (status != cudaSuccess) {
        throw std::runtime_error(std::string(operation) + ": " +
                                 cudaGetErrorString(status));
    }
}

__global__ void shade_pixel(const ScreenVertex* vertices,
                            std::size_t triangle_count, Color* colors,
                            float* depths, std::uint32_t width,
                            std::uint32_t height) {
    const std::size_t tid =
        static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (tid >= static_cast<std::size_t>(width) * height) return;

    const Vec3 sample{static_cast<float>(tid % width) + 0.5f,
                      static_cast<float>(tid / width) + 0.5f, 0.0f};
    float best_depth = depths[tid];
    Color best_color = colors[tid];
    for (std::size_t i = 0; i < triangle_count; ++i) {
        const ScreenVertex* v = vertices + i * 3;
        const Triangle triangle{v[0], v[1], v[2]};
        if (!cover_pixel(triangle, sample)) continue;

        const auto weights =
            barycentric(v[0].position, v[1].position, v[2].position, sample);
        const float depth = interpolate_depth(triangle, weights);
        if (depth < best_depth) {
            best_depth = depth;
            best_color = interpolate_color(triangle, weights);
        }
    }
    colors[tid] = best_color;
    depths[tid] = best_depth;
}

}  // namespace

void render_cuda(Framebuffer& target, const RenderInput& input) {
    const auto triangle_count = input.triangles.triangle_count();
    if (triangle_count == 0) return;

    for (std::size_t i = 0; i < triangle_count; ++i) {
        const ScreenVertex* v = input.triangles.triangle(i);
        const Triangle triangle{v[0], v[1], v[2]};
        if (!is_valid_screen_triangle(triangle)) {
            throw std::runtime_error("Triangle has invalid vertex");
        }
        if (!has_uniform_alpha(triangle) && !is_degenerate(triangle)) {
            throw std::logic_error(
                "CUDA rasterizer does not support alpha interpolation");
        }
    }

    const auto width = target.width();
    const auto height = target.height();
    const auto pixel_count = static_cast<std::size_t>(width) * height;

    constexpr unsigned threads = 256;
    const auto block_count = (pixel_count - 1) / threads + 1;
    if (block_count > std::numeric_limits<unsigned>::max()) {
        throw std::length_error("framebuffer needs too many CUDA blocks");
    }
    const auto blocks = static_cast<unsigned>(block_count);

    const auto vertex_bytes = triangle_count * 3 * sizeof(ScreenVertex);
    const auto color_bytes = pixel_count * sizeof(Color);
    const auto depth_bytes = pixel_count * sizeof(float);

    std::vector<Color> host_colors(pixel_count);
    std::vector<float> host_depths(pixel_count);
    ScreenVertex* d_vertices = nullptr;
    Color* d_colors = nullptr;
    float* d_depths = nullptr;

    const auto free_buffers = [&] {
        return std::array{cudaFree(d_depths), cudaFree(d_colors),
                          cudaFree(d_vertices)};
    };

    try {
        check_cuda(cudaMalloc(&d_vertices, vertex_bytes), "allocate vertices");
        check_cuda(cudaMalloc(&d_colors, color_bytes), "allocate colors");
        check_cuda(cudaMalloc(&d_depths, depth_bytes), "allocate depths");

        check_cuda(cudaMemcpy(d_vertices, input.triangles.vertices.data(),
                              vertex_bytes, cudaMemcpyHostToDevice),
                   "upload vertices");
        check_cuda(cudaMemcpy(d_colors, target.pixels_.data(), color_bytes,
                              cudaMemcpyHostToDevice),
                   "upload colors");
        check_cuda(cudaMemcpy(d_depths, target.depths_.data(), depth_bytes,
                              cudaMemcpyHostToDevice),
                   "upload depths");

        shade_pixel<<<blocks, threads>>>(d_vertices, triangle_count, d_colors,
                                         d_depths, width, height);
        check_cuda(cudaGetLastError(), "launch shade_pixel");

        check_cuda(cudaMemcpy(host_colors.data(), d_colors, color_bytes,
                              cudaMemcpyDeviceToHost),
                   "download colors");
        check_cuda(cudaMemcpy(host_depths.data(), d_depths, depth_bytes,
                              cudaMemcpyDeviceToHost),
                   "download depths");
    } catch (...) {
        free_buffers();
        throw;
    }

    for (const auto status : free_buffers()) {
        check_cuda(status, "cudaFree");
    }
    // 两个缓冲区都成功回读后，再一起更新 framebuffer。
    std::copy(host_colors.begin(), host_colors.end(), target.pixels_.begin());
    std::copy(host_depths.begin(), host_depths.end(), target.depths_.begin());
}

}  // namespace minigpu
