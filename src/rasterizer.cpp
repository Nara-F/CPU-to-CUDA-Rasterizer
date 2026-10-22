#include "rasterizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "cuda_backend.hpp"
#include "raster_math.hpp"

namespace {
minigpu::Color interpolate_color(const minigpu::Triangle& tri,
                                 const minigpu::Barycentric& bc) {
    minigpu::Color result;
    if (tri.a.color.a != tri.b.color.a || tri.a.color.a != tri.c.color.a ||
        tri.b.color.a != tri.c.color.a) {
        throw(std::logic_error(
            "CPU rasterizer does not support alpha interpolation"));
    }
    result.a = tri.a.color.a;
    result.r =
        tri.a.color.r * bc.w0 + tri.b.color.r * bc.w1 + tri.c.color.r * bc.w2;
    result.g =
        tri.a.color.g * bc.w0 + tri.b.color.g * bc.w1 + tri.c.color.g * bc.w2;
    result.b =
        tri.a.color.b * bc.w0 + tri.b.color.b * bc.w1 + tri.c.color.b * bc.w2;
    return result;
}

bool has_invalid_vertex(const minigpu::Triangle& triangle) {
    const auto& a = triangle.a.position;
    const auto& b = triangle.b.position;
    const auto& c = triangle.c.position;
    bool badA =
        !std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z);
    bool badB =
        !std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.z);
    bool badC =
        !std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z);
    return badA || badB || badC;
}

int32_t validate_cast_boundary(float value, uint32_t extent) {
    if (!std::isfinite(value)) {
        throw std::runtime_error("coordinate is not finite");
    }
    if (extent > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        throw std::runtime_error("framebuffer dimension is too large");
    }

    double boundary = std::ceil(static_cast<double>(value) - 0.5);
    boundary = std::clamp(boundary, 0.0, static_cast<double>(extent));
    return static_cast<int32_t>(boundary);
}

}  // namespace
namespace minigpu {

void CpuRasterizer::draw_triangle(const Triangle& triangle) {
    if (has_invalid_vertex(triangle)) {
        throw std::runtime_error("Triangle has invalid vertex");
    }
    if (minigpu::is_degenerate(triangle)) return;

    auto bounds = minigpu::triangle_bounds(triangle);
    auto& [min_x, min_y, max_x, max_y] = bounds;

    int32_t x_begin = validate_cast_boundary(min_x, target_.width());
    int32_t x_end = validate_cast_boundary(max_x, target_.width());
    int32_t y_begin = validate_cast_boundary(min_y, target_.height());
    int32_t y_end = validate_cast_boundary(max_y, target_.height());

    for (int32_t y = y_begin; y < y_end; ++y) {
        for (int32_t x = x_begin; x < x_end; ++x) {
            Vec3 p{x + 0.5f, y + 0.5f, 0.0f};
            if (minigpu::cover_pixel(triangle, p)) {
                auto bc = minigpu::barycentric(triangle.a.position,
                                               triangle.b.position,
                                               triangle.c.position, p);
                auto c = interpolate_color(triangle, bc);
                target_.set_pixel(x, y, c);
            }
        }
    }
}

void minigpu::CpuRasterizer::draw_triangle_scanLine(const Triangle& triangle) {
    if (has_invalid_vertex(triangle)) {
        throw std::runtime_error("Triangle has invalid vertex");
    }

    auto [v1, c1] = triangle.a;
    auto [v2, c2] = triangle.b;
    auto [v3, c3] = triangle.c;

    if (is_degenerate(triangle)) return;

    if (v1.y > v2.y) std::swap(v1, v2), std::swap(c1, c2);
    if (v1.y > v3.y) std::swap(v1, v3), std::swap(c1, c3);
    if (v2.y > v3.y) std::swap(v2, v3), std::swap(c2, c3);

    auto newTri = Triangle{{v1, c1}, {v2, c2}, {v3, c3}};

    int32_t y_begin = validate_cast_boundary(v1.y, target_.height());
    int32_t y_end = validate_cast_boundary(v3.y, target_.height());

    float slope2 = (v3.x - v1.x) / (v3.y - v1.y);

    for (int32_t y = y_begin; y < y_end; y++) {
        float x1;
        float sampleY = y + 0.5f;
        // Calculate only the active short edge. For flat tops the upper branch
        // is never visited; for flat bottoms the lower branch is never visited.
        // Thus neither horizontal edge causes a division by zero.
        if (sampleY < v2.y) {
            float slope1 = (v2.x - v1.x) / (v2.y - v1.y);
            x1 = v1.x + slope1 * (sampleY - v1.y);
        } else {
            float slope3 = (v3.x - v2.x) / (v3.y - v2.y);
            x1 = v2.x + slope3 * (sampleY - v2.y);
        }
        float x2 = v1.x + slope2 * (sampleY - v1.y);
        if (x1 > x2) std::swap(x1, x2);
        int32_t x_begin = validate_cast_boundary(x1, target_.width());
        int32_t x_end = validate_cast_boundary(x2, target_.width());

        for (int32_t x = x_begin; x < x_end; ++x) {
            auto bc = barycentric(v1, v2, v3, {x + 0.5f, sampleY, 0.0f});
            auto c = interpolate_color(newTri, bc);
            target_.set_pixel(x, y, c);
        }
    }

}  // scanLine implementation

void CpuRasterizer::export_ppm(const Framebuffer& fb,
                               const std::filesystem::path& path) {
    fb.write_ppm(path);
}

#if !MINIGPU_HAS_CUDA
void run_cuda_demo(Framebuffer&) {
    throw std::logic_error(
        "CUDA backend is unavailable; configure with -DMINIGPU_ENABLE_CUDA=ON "
        "after installing CUDA");
}
#endif

}  // namespace minigpu
