#include "raster_math.hpp"

#include <algorithm>
#include <cmath>

namespace minigpu {

bool is_valid_screen_triangle(const Triangle& t) {
    const auto valid_position = [](const Vec3& p) {
        return std::isfinite(p.x) && std::isfinite(p.y) &&
               p.z >= 0.0f && p.z <= 1.0f;
    };
    return valid_position(t.a.position) && valid_position(t.b.position) &&
           valid_position(t.c.position);
}

ScreenBounds triangle_bounds(const Triangle& t) {
    const Vec3& a = t.a.position;
    const Vec3& b = t.b.position;
    const Vec3& c = t.c.position;
    return {std::min({a.x, b.x, c.x}), std::min({a.y, b.y, c.y}),
            std::max({a.x, b.x, c.x}), std::max({a.y, b.y, c.y})};
}

bool is_inside_clip(const Vec4& clip) {
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
        !std::isfinite(clip.z) || !std::isfinite(clip.w))
        return false;
    if (!(clip.w > 0.0f)) return false;

    // 投影矩阵乘法在近 / 远面附近会引入约 1 ULP 的误差。严格比较会把
    // "正好贴在边界上"的顶点（例如 z_view = near）误判为越界；这里给
    // 相对 w 的 1e-6 容差，边界仍然算在裁剪体内。
    const float w = clip.w;
    const float tolerance = 1e-6f * w;
    return clip.x >= -w - tolerance && clip.x <= w + tolerance &&
           clip.y >= -w - tolerance && clip.y <= w + tolerance &&
           clip.z >= -w - tolerance && clip.z <= w + tolerance;
}

bool clip_to_screen(const Vec4& clip, std::uint32_t width,
                    std::uint32_t height, Vec3& screen) {
    if (!is_inside_clip(clip)) return false;

    const float inv_w = 1.0f / clip.w;
    // 裁剪判定允许边界舍入误差；映射前将其收回合法 NDC 范围，
    // 确保屏幕坐标和深度满足光栅器的输入契约。
    const float ndc_x = std::clamp(clip.x * inv_w, -1.0f, 1.0f);
    const float ndc_y = std::clamp(clip.y * inv_w, -1.0f, 1.0f);
    const float ndc_z = std::clamp(clip.z * inv_w, -1.0f, 1.0f);

    screen.x = (ndc_x + 1.0f) * 0.5f * static_cast<float>(width);
    screen.y = (1.0f - ndc_y) * 0.5f * static_cast<float>(height);
    screen.z = (ndc_z + 1.0f) * 0.5f;
    return true;
}

bool project_triangle(const Vec4& a, const Vec4& b, const Vec4& c,
                      std::uint32_t width, std::uint32_t height,
                      Vec3& screen_a, Vec3& screen_b, Vec3& screen_c) {
    // 契约：任一顶点越界则整三角形丢弃。先全部判定，再写输出。
    if (!is_inside_clip(a) || !is_inside_clip(b) || !is_inside_clip(c))
        return false;
    return clip_to_screen(a, width, height, screen_a) &&
           clip_to_screen(b, width, height, screen_b) &&
           clip_to_screen(c, width, height, screen_c);
}

}  // namespace minigpu
