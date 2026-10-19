#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "minigpu.hpp"

namespace {

constexpr float kTolerance = 1e-5f;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void check_close(float actual, float expected, const std::string& message) {
    if (std::fabs(actual - expected) > kTolerance) {
        std::ostringstream error;
        error << message << ": expected " << expected << ", got " << actual;
        throw std::runtime_error(error.str());
    }
}

minigpu::Triangle make_triangle(const minigpu::Vec3& a, const minigpu::Vec3& b,
                                const minigpu::Vec3& c) {
    minigpu::Triangle triangle;
    triangle.a.position = a;
    triangle.b.position = b;
    triangle.c.position = c;
    return triangle;
}

// 像素 (x,y) 的中心：屏幕原点左上，像素中心为 (x+0.5, y+0.5)。
minigpu::Vec3 pixel_center(int x, int y) {
    return {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, 0.0f};
}

void test_edge_function() {
    const minigpu::Vec3 a = {0.0f, 0.0f, 0.0f};
    const minigpu::Vec3 b = {10.0f, 0.0f, 0.0f};

    check_close(minigpu::edge_function(a, b, {5.0f, 3.0f, 0.0f}), 30.0f,
                "点在一侧时为正");
    check_close(minigpu::edge_function(a, b, {5.0f, -3.0f, 0.0f}), -30.0f,
                "点在另一侧时为负");
    check_close(minigpu::edge_function(a, b, {5.0f, 0.0f, 0.0f}), 0.0f,
                "点落在线上时为零");

    // 恒等式：E(b,c,p) + E(c,a,p) + E(a,b,p) == E(a,b,c)
    const minigpu::Vec3 c = {0.0f, 10.0f, 0.0f};
    const minigpu::Vec3 samples[] = {
        {1.0f, 1.0f, 0.0f}, {4.0f, 2.0f, 0.0f}, {-3.0f, 6.0f, 0.0f}};
    for (const minigpu::Vec3& p : samples) {
        check_close(minigpu::edge_function(b, c, p) +
                        minigpu::edge_function(c, a, p) +
                        minigpu::edge_function(a, b, p),
                    minigpu::edge_function(a, b, c), "三条边函数之和等于面积");
    }
}

void test_signed_area_and_winding() {
    const minigpu::Vec3 a = {0.0f, 0.0f, 0.0f};
    const minigpu::Vec3 b = {10.0f, 0.0f, 0.0f};
    const minigpu::Vec3 c = {0.0f, 10.0f, 0.0f};

    check_close(minigpu::signed_area(a, b, c), 100.0f, "该绕序面积为正");
    check_close(minigpu::signed_area(a, c, b), -100.0f, "交换两顶点面积反号");
    check_close(minigpu::signed_area(a, b, c) * 0.5f, 50.0f,
                "三角形面积是边函数的一半");
}

void test_degenerate() {
    check(minigpu::is_degenerate(
              make_triangle({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
                            {2.0f, 2.0f, 0.0f})),
          "三点共线是退化三角形");
    check(minigpu::is_degenerate(
              make_triangle({3.0f, 4.0f, 0.0f}, {3.0f, 4.0f, 0.0f},
                            {5.0f, 6.0f, 0.0f})),
          "两点重合是退化三角形");
    check(!minigpu::is_degenerate(make_triangle(
              {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {0.0f, 10.0f, 0.0f})),
          "正常三角形不是退化");

    const minigpu::Triangle flat = make_triangle(
        {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {2.0f, 2.0f, 0.0f});
    check(!minigpu::cover_pixel(flat, {1.0f, 0.5f, 0.0f}),
          "退化三角形不覆盖任何像素");
}

void test_top_left_rule() {
    const minigpu::Vec3 a = {0.0f, 0.0f, 0.0f};
    const minigpu::Vec3 b = {10.0f, 0.0f, 0.0f};
    const minigpu::Vec3 c = {0.0f, 10.0f, 0.0f};

    check(minigpu::is_top_left(a, b), "顶边（水平朝右）是 top-left");
    check(minigpu::is_top_left(c, a), "左边（竖直朝上）是 top-left");
    check(!minigpu::is_top_left(b, c), "斜边不是 top-left");
    check(!minigpu::is_top_left(b, a), "反过来的顶边不是 top-left");
}

void test_inside_edge() {
    const minigpu::Vec3 a = {0.0f, 0.0f, 0.0f};
    const minigpu::Vec3 b = {10.0f, 0.0f, 0.0f};

    check(minigpu::inside_edge(5.0f, a, b, 1.0f), "正边函数值在内侧");
    check(!minigpu::inside_edge(-5.0f, a, b, 1.0f), "负边函数值在外侧");
    check(minigpu::inside_edge(0.0f, a, b, 1.0f), "零值且是 top-left 则收入");
    check(!minigpu::inside_edge(0.0f, b, a, 1.0f), "零值但不是 top-left 则排除");
    check(minigpu::inside_edge(-5.0f, a, b, -1.0f), "sign 翻转后负值算内侧");
}

void test_barycentric() {
    const minigpu::Vec3 a = {0.0f, 0.0f, 0.0f};
    const minigpu::Vec3 b = {10.0f, 0.0f, 0.0f};
    const minigpu::Vec3 c = {0.0f, 10.0f, 0.0f};

    const minigpu::Barycentric at_a = minigpu::barycentric(a, b, c, a);
    check_close(at_a.w0, 1.0f, "顶点 a 处 w0");
    check_close(at_a.w1, 0.0f, "顶点 a 处 w1");
    check_close(at_a.w2, 0.0f, "顶点 a 处 w2");

    const minigpu::Barycentric at_b = minigpu::barycentric(a, b, c, b);
    check_close(at_b.w1, 1.0f, "顶点 b 处 w1");

    const minigpu::Barycentric at_c = minigpu::barycentric(a, b, c, c);
    check_close(at_c.w2, 1.0f, "顶点 c 处 w2");

    // 内部点：三者之和为 1，且能重建出原坐标。
    const minigpu::Vec3 p = {2.0f, 2.0f, 0.0f};
    const minigpu::Barycentric w = minigpu::barycentric(a, b, c, p);
    check_close(w.w0 + w.w1 + w.w2, 1.0f, "重心坐标之和为 1");
    check_close(w.w0 * a.x + w.w1 * b.x + w.w2 * c.x, p.x, "用 w 重建 x");
    check_close(w.w0 * a.y + w.w1 * b.y + w.w2 * c.y, p.y, "用 w 重建 y");
}

void test_triangle_bounds() {
    const minigpu::ScreenBounds bounds = minigpu::triangle_bounds(
        make_triangle({1.0f, 2.0f, 0.0f}, {5.0f, 2.0f, 0.0f},
                      {3.0f, 7.0f, 0.0f}));
    check_close(bounds.min_x, 1.0f, "包围盒 min_x");
    check_close(bounds.min_y, 2.0f, "包围盒 min_y");
    check_close(bounds.max_x, 5.0f, "包围盒 max_x");
    check_close(bounds.max_y, 7.0f, "包围盒 max_y");
}

void test_coverage() {
    const minigpu::Triangle triangle = make_triangle(
        {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {0.0f, 10.0f, 0.0f});

    check(minigpu::cover_pixel(triangle, pixel_center(1, 1)),
          "内部像素被覆盖");
    check(!minigpu::cover_pixel(triangle, pixel_center(8, 8)),
          "外部像素不被覆盖");
    check(!minigpu::cover_pixel(triangle, pixel_center(-1, 0)),
          "负坐标不被覆盖");
}

// 交换两个顶点（绕序反转）后，覆盖结果必须逐个像素完全一致。
void test_reversed_winding() {
    const minigpu::Vec3 a = {0.0f, 0.0f, 0.0f};
    const minigpu::Vec3 b = {10.0f, 0.0f, 0.0f};
    const minigpu::Vec3 c = {0.0f, 10.0f, 0.0f};
    const minigpu::Triangle cw = make_triangle(a, b, c);
    const minigpu::Triangle ccw = make_triangle(a, c, b);

    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            const minigpu::Vec3 p = pixel_center(x, y);
            if (minigpu::cover_pixel(cw, p) != minigpu::cover_pixel(ccw, p)) {
                std::ostringstream error;
                error << "像素 (" << x << ", " << y
                      << ") 在反转绕序后覆盖结果变了";
                throw std::runtime_error(error.str());
            }
        }
    }
}

// 两个三角形拼成 10x10 矩形：每个内部像素必须恰好被覆盖一次。
// 对角线 x + y = 10 正好穿过 10 个像素中心 (x+0.5, 9-x+0.5)，
// 所以这条同时验证了 top-left 规则：漏一次是缝，多一次是重复。
void test_two_triangles_fill_rectangle() {
    const minigpu::Triangle t1 = make_triangle(
        {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {0.0f, 10.0f, 0.0f});
    const minigpu::Triangle t2 = make_triangle(
        {10.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 0.0f}, {0.0f, 10.0f, 0.0f});

    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            const minigpu::Vec3 p = pixel_center(x, y);
            const int hits = (minigpu::cover_pixel(t1, p) ? 1 : 0) +
                             (minigpu::cover_pixel(t2, p) ? 1 : 0);
            if (hits != 1) {
                std::ostringstream error;
                error << "像素 (" << x << ", " << y << ") 被覆盖 " << hits
                      << " 次，应该恰好 1 次";
                throw std::runtime_error(error.str());
            }
        }
    }

    check(!minigpu::cover_pixel(t1, pixel_center(10, 10)) &&
              !minigpu::cover_pixel(t2, pixel_center(10, 10)),
          "矩形外部不应被覆盖");

    // 钉住 top-left 的方向约定。上面的"恰好一次"抓不到整体反转——
    // 规则反了以后，共享边仍然只被一方认领，只是换了一方。
    // 所以要单独断言归属，而不能只验总数。
    const minigpu::Vec3 on_diagonal = {0.5f, 9.5f, 0.0f};
    check(!minigpu::cover_pixel(t1, on_diagonal),
          "共享对角线上的像素不应归 t1");
    check(minigpu::cover_pixel(t2, on_diagonal),
          "共享对角线上的像素应归 t2");
}

}  // namespace

int main() {
    try {
        test_edge_function();
        test_signed_area_and_winding();
        test_degenerate();
        test_top_left_rule();
        test_inside_edge();
        test_barycentric();
        test_triangle_bounds();
        test_coverage();
        test_reversed_winding();
        test_two_triangles_fill_rectangle();
    } catch (const std::exception& error) {
        std::cerr << "raster_math_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
