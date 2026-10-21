#include "rasterizer.hpp"

#include <cmath>
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
}  // namespace
namespace minigpu {

void CpuRasterizer::draw_triangle(const Triangle&) {
    throw std::logic_error(
        "CPU triangle rasterization is a lab TODO (barycentrics, depth, "
        "interpolation)");
}

void minigpu::CpuRasterizer::draw_triangle_scanLine(const Triangle& triangle) {
    auto [v1, c1] = triangle.a;
    auto [v2, c2] = triangle.b;
    auto [v3, c3] = triangle.c;

    // Reject invalid screen coordinates before division or integer conversion.
    const auto finite_xy = [](const Vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y);
    };
    if (!finite_xy(v1) || !finite_xy(v2) || !finite_xy(v3)) return;

    // Reuse the shared zero-area rule: horizontal/vertical/diagonal
    // collinearity and every combination of coincident vertices are all
    // degenerate in x/y.
    if (!std::isfinite(signed_area(v1, v2, v3)) || is_degenerate(triangle))
        return;

    if (v1.y > v2.y) std::swap(v1, v2), std::swap(c1, c2);
    if (v1.y > v3.y) std::swap(v1, v3), std::swap(c1, c3);
    if (v2.y > v3.y) std::swap(v2, v3), std::swap(c2, c3);

    // A nondegenerate triangle has nonzero total height after sorting.
    float slope2 = (v3.x - v1.x) / (v3.y - v1.y);

    int32_t minY = std::ceil(v1.y - 0.5f);
    int32_t maxY = std::ceil(v3.y - 0.5f);

    for (int32_t y = minY; y < maxY; y++) {
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
        int32_t x_begin = std::ceil(x1 - 0.5f);
        int32_t x_end = std::ceil(x2 - 0.5f);

        for (int32_t x = x_begin; x < x_end; ++x) {
            auto bc = barycentric(v1, v2, v3, {x + 0.5f, sampleY, 0.0f});
            auto c = interpolate_color(triangle, bc);
            target_.set_pixel(x, y, c);
        }
    }

}  // scanLine implementation

void export_ppm(const Framebuffer& fb, const std::filesystem::path& path) {
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
