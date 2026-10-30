#include "raster_math.hpp"

#include <algorithm>
#include <cmath>

namespace minigpu {

// E(u,v,p) = (v-u) x (p-u) 的 z 分量。
// 恒等式：E(b,c,p) + E(c,a,p) + E(a,b,p) == E(a,b,c)。
float edge_function(const Vec3& a, const Vec3& b, const Vec3& p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// 有符号面积的两倍。屏幕空间 y 向下：> 0 视觉上顺时针。
float signed_area(const Vec3& a, const Vec3& b, const Vec3& c) {
    return edge_function(a, b, c);
}

bool is_degenerate(const Triangle& t) {
    return signed_area(t.a.position, t.b.position, t.c.position) == 0.0f;
}

// 只在 signed_area > 0 的规范朝向下按这个方向调用；
// area < 0 时由 inside_edge 把 a/b 反过来传。
bool is_top_left(const Vec3& a, const Vec3& b) {
    return (b.y == a.y && b.x > a.x) || (b.y < a.y);
}

bool inside_edge(float e, const Vec3& a, const Vec3& b, float sign) {
    if (e * sign > 0.0f) return true;
    if (e != 0.0f) return false;
    // e == 0：像素中心正好落在边上，按 top-left 规则归属。
    // 不交换顶点，只把边的方向反过来，避免重心坐标和顶点错位。
    return sign > 0.0f ? is_top_left(a, b) : is_top_left(b, a);
}

Barycentric barycentric(const Vec3& a, const Vec3& b, const Vec3& c,
                        const Vec3& p) {
    // 调用方必须先判退化，否则这里会除零。
    const float area = signed_area(a, b, c);
    return {edge_function(b, c, p) / area, edge_function(c, a, p) / area,
            edge_function(a, b, p) / area};
}

ScreenBounds triangle_bounds(const Triangle& t) {
    const Vec3& a = t.a.position;
    const Vec3& b = t.b.position;
    const Vec3& c = t.c.position;
    return {std::min({a.x, b.x, c.x}), std::min({a.y, b.y, c.y}),
            std::max({a.x, b.x, c.x}), std::max({a.y, b.y, c.y})};
}

bool cover_pixel(const Triangle& t, const Vec3& p) {
    const Vec3& a = t.a.position;
    const Vec3& b = t.b.position;
    const Vec3& c = t.c.position;

    const float area = signed_area(a, b, c);
    if (area == 0.0f) return false;

    // sign 把两种绕序统一成同一种内侧判定，且不交换顶点。
    const float sign = area > 0.0f ? 1.0f : -1.0f;
    return inside_edge(edge_function(b, c, p), b, c, sign) &&
           inside_edge(edge_function(c, a, p), c, a, sign) &&
           inside_edge(edge_function(a, b, p), a, b, sign);
}

bool is_inside_clip(const Vec4& clip) {
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
        !std::isfinite(clip.z) || !std::isfinite(clip.w))
        return false;
    if (!(clip.w > 0.0f)) return false;

    const float w = clip.w;
    return clip.x >= -w && clip.x <= w && clip.y >= -w && clip.y <= w &&
           clip.z >= -w && clip.z <= w;
}

bool clip_to_screen(const Vec4& clip, std::uint32_t width,
                    std::uint32_t height, Vec3& screen) {
    if (!is_inside_clip(clip)) return false;

    const float inv_w = 1.0f / clip.w;
    const float ndc_x = clip.x * inv_w;
    const float ndc_y = clip.y * inv_w;
    const float ndc_z = clip.z * inv_w;

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
