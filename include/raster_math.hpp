#pragma once

#include "types.hpp"

namespace minigpu {

float edge_function(const Vec3& a, const Vec3& b, const Vec3& p);

float signed_area(const Vec3& a, const Vec3& b, const Vec3& c);

bool is_degenerate(const Triangle& t);

bool is_top_left(const Vec3& a, const Vec3& b);

bool inside_edge(float e, const Vec3& a, const Vec3& b, float sign);

struct Barycentric {
    float w0 = 0, w1 = 0, w2 = 0;
};
// w0 对应顶点 a，w1 对应 b，w2 对应 c，三者之和为 1。
// 前置条件：!is_degenerate(a, b, c)；否则会除零。
Barycentric barycentric(const Vec3& a, const Vec3& b, const Vec3& c,
                        const Vec3& p);

struct ScreenBounds {
    float min_x = 0, min_y = 0, max_x = 0, max_y = 0;
};
// 屏幕空间浮点包围盒，不 clamp。
// clamp 到 [0, width-1] x [0, height-1] 是调用方（CPU/CUDA 后端）的责任。
ScreenBounds triangle_bounds(const Triangle& t);

bool cover_pixel(const Triangle& t, const Vec3& p);

}  // namespace minigpu