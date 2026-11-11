#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "minigpu.hpp"

namespace {
constexpr minigpu::Color kBackground{12, 23, 34, 255};
constexpr minigpu::Color kRed{230, 75, 65, 255};
constexpr minigpu::Color kBlue{30, 80, 220, 255};
using Draw = void (minigpu::CpuRasterizer::*)(const minigpu::Triangle&);
constexpr Draw kDraws[] = {&minigpu::CpuRasterizer::draw_triangle,
                           &minigpu::CpuRasterizer::draw_triangle_scanLine};

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool same_color(minigpu::Color a, minigpu::Color b, int tolerance = 0) {
    return std::abs(int(a.r) - int(b.r)) <= tolerance &&
           std::abs(int(a.g) - int(b.g)) <= tolerance &&
           std::abs(int(a.b) - int(b.b)) <= tolerance && a.a == b.a;
}

void check_depth(float actual, float expected) {
    check((std::isinf(expected) && actual == expected) ||
              std::fabs(actual - expected) <= 1e-5f,
          "unexpected depth: " + std::to_string(actual));
}

void check_empty(const minigpu::Framebuffer& fb) {
    for (unsigned y = 0; y < fb.height(); ++y) {
        for (unsigned x = 0; x < fb.width(); ++x) {
            check(same_color(fb.at(x, y), kBackground), "expected background");
            check_depth(fb.depth_at(x, y),
                        std::numeric_limits<float>::infinity());
        }
    }
}

minigpu::Triangle triangle(float depth, minigpu::Color color) {
    return {{{0, 0, depth}, color}, {{8, 0, depth}, color},
            {{0, 8, depth}, color}};
}

void draw(minigpu::Framebuffer& fb, const minigpu::Triangle& tri, Draw method) {
    // 每次创建新实例，验证深度生命周期属于 framebuffer。
    minigpu::CpuRasterizer rasterizer(fb);
    (rasterizer.*method)(tri);
}

void test_framebuffer_depth() {
    minigpu::Framebuffer fb(3, 2, kBackground);
    check_empty(fb);
    bool threw = false;
    try {
        (void)fb.depth_at(3, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "depth access must check bounds");
    check(fb.depth_test_and_set_pixel(1, 1, 1.0f, kBlue),
          "far plane must pass on an empty buffer");
    check(fb.depth_test_and_set_pixel(1, 1, 0.0f, kRed),
          "near plane must replace far plane");
    check(!fb.depth_test_and_set_pixel(1, 1, 0.0f, kBlue),
          "equal depth must keep first color");
    for (float invalid : {-0.1f, 1.1f,
                          std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity()}) {
        check(!fb.depth_test_and_set_pixel(0, 0, invalid, kBlue),
              "invalid fragment depth must be rejected");
    }
    check(!fb.depth_test_and_set_pixel(3, 0, 0.0f, kBlue),
          "out-of-bounds fragment must be rejected");
    check(same_color(fb.at(1, 1), kRed), "rejected write changed color");
    check_depth(fb.depth_at(1, 1), 0.0f);
    check_depth(fb.depth_at(0, 0), std::numeric_limits<float>::infinity());
    fb.clear(kBackground);
    check_empty(fb);
}

void test_occlusion(Draw first, Draw second) {
    for (bool reverse : {false, true}) {
        minigpu::Framebuffer fb(8, 8, kBackground);
        const auto near = triangle(0.25f, kRed);
        const auto far = triangle(0.75f, kBlue);
        draw(fb, reverse ? near : far, first);
        draw(fb, reverse ? far : near, second);
        for (unsigned y = 0; y < 8; ++y) {
            for (unsigned x = 0; x < 8; ++x) {
                const bool covered = x + y < 7;
                check(same_color(fb.at(x, y), covered ? kRed : kBackground),
                      "near triangle must win in either input order");
                check_depth(fb.depth_at(x, y),
                            covered ? 0.25f :
                                std::numeric_limits<float>::infinity());
            }
        }
    }
}

void test_interpolated_depth(Draw first, Draw second) {
    // 倾斜三角形的 z = screen_x / 8，与 z = 0.5 的平面在画面内相交。
    auto slope = triangle(0.0f, kRed);
    slope.b.position.z = 1.0f;
    const auto flat = triangle(0.5f, kBlue);
    for (bool reverse : {false, true}) {
        minigpu::Framebuffer fb(8, 8, kBackground);
        draw(fb, reverse ? slope : flat, first);
        draw(fb, reverse ? flat : slope, second);
        for (unsigned y = 0; y < 8; ++y) {
            for (unsigned x = 0; x + y < 7; ++x) {
                const float slope_depth = (x + 0.5f) / 8.0f;
                const bool red = slope_depth < 0.5f;
                check(same_color(fb.at(x, y), red ? kRed : kBlue),
                      "intersecting planes need per-pixel depth");
                check_depth(fb.depth_at(x, y), std::min(slope_depth, 0.5f));
            }
        }
    }
}

void test_equal_depth_and_clear(Draw first, Draw second) {
    for (bool reverse : {false, true}) {
        minigpu::Framebuffer fb(8, 8, kBackground);
        const auto first_color = reverse ? kRed : kBlue;
        draw(fb, triangle(0.5f, first_color), first);
        draw(fb, triangle(0.5f, reverse ? kBlue : kRed), second);
        check(same_color(fb.at(1, 1), first_color), "equal depth overwrote first");
        check_depth(fb.depth_at(1, 1), 0.5f);
        minigpu::CpuRasterizer rasterizer(fb);
        rasterizer.draw_clear(kBackground);
        check_empty(fb);
        draw(fb, triangle(0.75f, kRed), second);
        check(same_color(fb.at(1, 1), kRed), "clear left stale depth");
        check_depth(fb.depth_at(1, 1), 0.75f);
    }
}

void test_coplanar_triangles(Draw first, Draw second) {
    for (float depth : {0.3f, 0.5f, 1.0f}) {
        for (bool reverse : {false, true}) {
            auto large = triangle(depth, reverse ? kBlue : kRed);
            auto small = triangle(depth, reverse ? kRed : kBlue);
            small.b.position.x = 7;
            small.c.position.y = 7;
            minigpu::Framebuffer fb(8, 8, kBackground);
            draw(fb, reverse ? small : large, first);
            draw(fb, reverse ? large : small, second);
            for (unsigned y = 0; y < 6; ++y) {
                for (unsigned x = 0; x + y < 6; ++x) {
                    check(same_color(fb.at(x, y), kRed, 1),
                          "coplanar triangles must keep the first input");
                    check(fb.depth_at(x, y) == depth,
                          "constant vertex depth must remain constant");
                }
            }
        }
    }
}

void test_invalid_vertex_depth(Draw method) {
    for (float invalid : {-0.1f, 1.1f,
                          std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity()}) {
        for (unsigned i = 0; i < 3; ++i) {
            minigpu::Framebuffer fb(8, 8, kBackground);
            auto tri = triangle(0.5f, kRed);
            minigpu::ScreenVertex* vertices[] = {&tri.a, &tri.b, &tri.c};
            vertices[i]->position.z = invalid;
            bool threw = false;
            try {
                draw(fb, tri, method);
            } catch (const std::runtime_error&) {
                threw = true;
            }
            check(threw, "invalid vertex depth must throw before drawing");
            check_empty(fb);
        }
    }
}

void render(minigpu::Framebuffer& fb,
            const std::vector<minigpu::Triangle>& triangles, Draw method,
            bool reverse) {
    for (std::size_t i = 0; i < triangles.size(); ++i)
        draw(fb, triangles[reverse ? triangles.size() - 1 - i : i], method);
}

void check_same_image(const minigpu::Framebuffer& a,
                       const minigpu::Framebuffer& b) {
    for (unsigned y = 0; y < a.height(); ++y) {
        for (unsigned x = 0; x < a.width(); ++x) {
            check(same_color(a.at(x, y), b.at(x, y), 1),
                  "cube color/coverage differs at " + std::to_string(x) +
                      "," + std::to_string(y));
            check_depth(a.depth_at(x, y), b.depth_at(x, y));
        }
    }
}

void test_cube_front_face() {
    const auto projection = minigpu::perspective(minigpu::degrees(90), 1, 1, 10);
    const auto view = minigpu::look_at({0, 0, -5}, {0, 0, 0}, {0, 1, 0});
    const auto triangles = minigpu::make_cube_triangles_hardcoded(projection * view, 64, 64);
    check(triangles.size() == 12, "fully visible cube must have 12 triangles");
    for (auto method : kDraws) {
        for (bool reverse : {false, true}) {
            minigpu::Framebuffer fb(64, 64, kBackground);
            render(fb, triangles, method, reverse);
            for (unsigned y = 0; y < 64; ++y) {
                for (unsigned x = 0; x < 64; ++x) {
                    // 近侧面 z_view = 4，投影边界为 [24,40]，depth = 5/6。
                    const bool covered = x >= 24 && x < 40 && y >= 24 && y < 40;
                    check(same_color(fb.at(x, y), covered ? kRed : kBackground, 1),
                          "cube front face must hide its back face");
                    check_depth(fb.depth_at(x, y), covered ? 5.0f / 6.0f :
                                    std::numeric_limits<float>::infinity());
                }
            }
        }
    }
}

void test_rotated_cube() {
    for (auto size : {std::array<unsigned, 2>{96, 64}, {64, 96}}) {
        for (float angle : {0.0f, 30.0f, 75.0f}) {
            const auto p = minigpu::perspective(minigpu::degrees(60),
                float(size[0]) / float(size[1]), 0.1f, 100.0f);
            const auto v = minigpu::look_at({3, 2, -6}, {0, 0, 0}, {0, 1, 0});
            const auto m = minigpu::rotation_y(minigpu::degrees(angle));
            const auto triangles = minigpu::make_cube_triangles_hardcoded(p * v * m,
                                                               size[0], size[1]);
            check(triangles.size() == 12, "demo cube must be fully inside clip volume");
            minigpu::Framebuffer reference(size[0], size[1], kBackground);
            render(reference, triangles, kDraws[0], false);
            unsigned covered = 0;
            for (unsigned y = 0; y < size[1]; ++y)
                for (unsigned x = 0; x < size[0]; ++x)
                    covered += std::isfinite(reference.depth_at(x, y)) ? 1 : 0;
            check(covered > 100 && covered < size[0] * size[1],
                  "rotated cube must produce visible pixels and background");
            for (auto method : kDraws) {
                for (bool reverse : {false, true}) {
                    minigpu::Framebuffer fb(size[0], size[1], kBackground);
                    render(fb, triangles, method, reverse);
                    check_same_image(reference, fb);
                }
            }
        }
    }
}

void test_cube_clip_rejection() {
    const auto p = minigpu::perspective(minigpu::degrees(60), 1, 0.1f, 100);
    check(minigpu::make_cube_triangles_hardcoded(p * minigpu::translation({0, 0, -5}),
                                       64, 64).empty(),
          "cube behind camera must be rejected");
    minigpu::Mat4 zero;
    for (auto& column : zero.m)
        for (auto& value : column) value = 0;
    check(minigpu::make_cube_triangles_hardcoded(zero, 64, 64).empty(),
          "zero w must be rejected before perspective division");
    minigpu::Mat4 invalid;
    invalid.m[0][0] = std::numeric_limits<float>::quiet_NaN();
    check(minigpu::make_cube_triangles_hardcoded(invalid, 64, 64).empty(),
          "nonfinite transformed coordinates must be rejected");
}
}  // namespace

int main() {
    try {
        test_framebuffer_depth();
        for (auto first : kDraws) {
            test_invalid_vertex_depth(first);
            for (auto second : kDraws) {
                test_occlusion(first, second);
                test_interpolated_depth(first, second);
                test_equal_depth_and_clear(first, second);
                test_coplanar_triangles(first, second);
            }
        }
        test_cube_front_face();
        test_rotated_cube();
        test_cube_clip_rejection();
    } catch (const std::exception& error) {
        std::cerr << "depth_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
