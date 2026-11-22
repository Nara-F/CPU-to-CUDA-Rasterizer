#pragma once

#include "types.hpp"

#if defined(__CUDACC__) || defined(__CUDA__)
#define MINIGPU_HOST_DEVICE __host__ __device__
#else
#define MINIGPU_HOST_DEVICE
#endif

namespace minigpu {

// E(u,v,p) = (v-u) x (p-u) 的 z 分量。
// E(b,c,p) + E(c,a,p) + E(a,b,p) == E(a,b,c)。
MINIGPU_HOST_DEVICE inline float edge_function(const Vec3& a, const Vec3& b,
                                              const Vec3& p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// 屏幕空间 y 向下，正面积在视觉上是顺时针。
MINIGPU_HOST_DEVICE inline float signed_area(const Vec3& a, const Vec3& b,
                                             const Vec3& c) {
    return edge_function(a, b, c);
}

inline bool is_degenerate(const Triangle& t) {
    return signed_area(t.a.position, t.b.position, t.c.position) == 0.0f;
}

MINIGPU_HOST_DEVICE inline bool is_top_left(const Vec3& a, const Vec3& b) {
    return (b.y == a.y && b.x > a.x) || (b.y < a.y);
}

MINIGPU_HOST_DEVICE inline bool inside_edge(float e, const Vec3& a,
                                            const Vec3& b, float sign) {
    if (e * sign > 0.0f) return true;
    if (e != 0.0f) return false;
    // 用绕序确定边的归属，并保留顶点与重心坐标的对应关系。
    return sign > 0.0f ? is_top_left(a, b) : is_top_left(b, a);
}

MINIGPU_HOST_DEVICE inline bool cover_pixel(const Triangle& t, const Vec3& p) {
    const Vec3& a = t.a.position;
    const Vec3& b = t.b.position;
    const Vec3& c = t.c.position;
    const float area = signed_area(a, b, c);
    if (area == 0.0f) return false;

    const float sign = area > 0.0f ? 1.0f : -1.0f;
    return inside_edge(edge_function(b, c, p), b, c, sign) &&
           inside_edge(edge_function(c, a, p), c, a, sign) &&
           inside_edge(edge_function(a, b, p), a, b, sign);
}

struct Barycentric {
    float w0 = 0, w1 = 0, w2 = 0;
};
// w0 对应顶点 a，w1 对应 b，w2 对应 c，三者之和为 1。
// 前置条件：三角形非退化。
MINIGPU_HOST_DEVICE inline Barycentric barycentric(const Vec3& a, const Vec3& b,
                                                   const Vec3& c, const Vec3& p) {
    const float area = signed_area(a, b, c);
    return {edge_function(b, c, p) / area, edge_function(c, a, p) / area,
            edge_function(a, b, p) / area};
}

MINIGPU_HOST_DEVICE inline float interpolate_depth(const Triangle& t,
                                                   const Barycentric& bc) {
    // 以 a 为基准插值，使三个顶点等深时的结果保持精确等深。
    const float z = t.a.position.z;
    const float depth = z + (t.b.position.z - z) * bc.w1 +
                        (t.c.position.z - z) * bc.w2;
    if (depth < 0.0f) return 0.0f;
    return depth > 1.0f ? 1.0f : depth;
}

inline bool has_uniform_alpha(const Triangle& t) {
    return t.a.color.a == t.b.color.a && t.a.color.a == t.c.color.a;
}

// 调用方负责校验三个顶点的 alpha 一致。
MINIGPU_HOST_DEVICE inline Color interpolate_color(const Triangle& t,
                                                   const Barycentric& bc) {
    Color result;
    result.r = t.a.color.r * bc.w0 + t.b.color.r * bc.w1 + t.c.color.r * bc.w2;
    result.g = t.a.color.g * bc.w0 + t.b.color.g * bc.w1 + t.c.color.g * bc.w2;
    result.b = t.a.color.b * bc.w0 + t.b.color.b * bc.w1 + t.c.color.b * bc.w2;
    result.a = t.a.color.a;
    return result;
}

bool is_valid_screen_triangle(const Triangle& t);

struct ScreenBounds {
    float min_x = 0, min_y = 0, max_x = 0, max_y = 0;
};
// 屏幕空间浮点包围盒，不 clamp。
// clamp 到 [0, width-1] x [0, height-1] 是调用方（CPU/CUDA 后端）的责任。
ScreenBounds triangle_bounds(const Triangle& t);

// 裁剪体判定：坐标有限、w > 0，且 -w <= x,y,z <= w。
// 边界使用相对 w 的 1e-6 容差，避免投影矩阵的浮点舍入把近 / 远面上的
// 顶点误判为越界。
bool is_inside_clip(const Vec4& clip);

// 透视除法 + viewport 映射。返回 false 表示顶点被裁剪拒绝，此时 screen 不变。
// 容差内的边界误差在 NDC 中钳制到 [-1,1]，确保输出深度位于 [0,1]。
// screen.z 是归一化深度 (ndc_z + 1) / 2，近 0 远 1。
bool clip_to_screen(const Vec4& clip, std::uint32_t width,
                    std::uint32_t height, Vec3& screen);

// 契约：不做完整齐次裁剪。三个顶点都在裁剪体内才返回 true 并写入屏幕坐标；
// 任一越界则整三角形丢弃，所有输出参数保持不变。
bool project_triangle(const Vec4& a, const Vec4& b, const Vec4& c,
                      std::uint32_t width, std::uint32_t height,
                      Vec3& screen_a, Vec3& screen_b, Vec3& screen_c);

}  // namespace minigpu

#undef MINIGPU_HOST_DEVICE
