#include "rasterizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "raster_math.hpp"
#include "render_entry.hpp"

namespace {
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

void render_cpu(Framebuffer& target, const RenderInput& input) {
    CpuRasterizer rasterizer(target);
    for (std::size_t i = 0; i < input.triangles.triangle_count(); ++i) {
        const auto* vertices = input.triangles.triangle(i);
        rasterizer.draw_triangle({vertices[0], vertices[1], vertices[2]});
    }
}

void CpuRasterizer::shade_pixel(const Triangle& triangle, std::uint32_t x,
                                std::uint32_t y) {
    const auto bc =
        barycentric(triangle.a.position, triangle.b.position,
                    triangle.c.position, {x + 0.5f, y + 0.5f, 0.0f});
    const float depth = interpolate_depth(triangle, bc);
    if (!has_uniform_alpha(triangle)) {
        throw std::logic_error(
            "CPU rasterizer does not support alpha interpolation");
    }
    const auto color = interpolate_color(triangle, bc);
    target_.depth_test_and_set_pixel(x, y, depth, color);
}

void CpuRasterizer::draw_triangle(const Triangle& triangle) {
    if (!is_valid_screen_triangle(triangle)) {
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
                shade_pixel(triangle, x, y);
            }
        }
    }
}

void minigpu::CpuRasterizer::draw_triangle_scanLine(const Triangle& triangle) {
    if (!is_valid_screen_triangle(triangle)) {
        throw std::runtime_error("Triangle has invalid vertex");
    }

    auto v1 = triangle.a.position;
    auto v2 = triangle.b.position;
    auto v3 = triangle.c.position;

    if (is_degenerate(triangle)) return;

    if (v1.y > v2.y) std::swap(v1, v2);
    if (v1.y > v3.y) std::swap(v1, v3);
    if (v2.y > v3.y) std::swap(v2, v3);

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
            // 保留原始顶点顺序插值，使两种实现的等深判定一致。
            shade_pixel(triangle, x, y);
        }
    }

}  // scanLine implementation

void CpuRasterizer::export_ppm(const Framebuffer& fb,
                               const std::filesystem::path& path) {
    fb.write_ppm(path);
}

}  // namespace minigpu
