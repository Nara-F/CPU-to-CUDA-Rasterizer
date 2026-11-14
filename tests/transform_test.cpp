#include <cmath>
#include <iostream>
#include <limits>
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
    if (!std::isfinite(actual) || !std::isfinite(expected) ||
        std::fabs(actual - expected) > kTolerance) {
        std::ostringstream error;
        error << message << ": expected " << expected << ", got " << actual;
        throw std::runtime_error(error.str());
    }
}

void check_close(const minigpu::Vec3& actual, const minigpu::Vec3& expected,
                 const std::string& message) {
    check_close(actual.x, expected.x, message + " (x)");
    check_close(actual.y, expected.y, message + " (y)");
    check_close(actual.z, expected.z, message + " (z)");
}

void check_close(const minigpu::Vec4& actual, const minigpu::Vec4& expected,
                 const std::string& message) {
    check_close(actual.x, expected.x, message + " (x)");
    check_close(actual.y, expected.y, message + " (y)");
    check_close(actual.z, expected.z, message + " (z)");
    check_close(actual.w, expected.w, message + " (w)");
}

template <typename Fn>
bool throws_invalid_argument(Fn&& fn) {
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void test_clip_boundary_is_inclusive() {
    // 契约：-w <= x,y,z <= w，边界本身算在裁剪体内。
    check(minigpu::is_inside_clip({1.0f, 0.0f, 0.0f, 1.0f}), "x = +w");
    check(minigpu::is_inside_clip({-1.0f, 0.0f, 0.0f, 1.0f}), "x = -w");
    check(minigpu::is_inside_clip({0.0f, 1.0f, 0.0f, 1.0f}), "y = +w");
    check(minigpu::is_inside_clip({0.0f, -1.0f, 0.0f, 1.0f}), "y = -w");
    check(minigpu::is_inside_clip({0.0f, 0.0f, 1.0f, 1.0f}), "z = +w");
    check(minigpu::is_inside_clip({0.0f, 0.0f, -1.0f, 1.0f}), "z = -w");
    check(minigpu::is_inside_clip({1.0f, 1.0f, 1.0f, 1.0f}), "corner");
    check(!minigpu::is_inside_clip({1.001f, 0.0f, 0.0f, 1.0f}),
          "just outside +w");
    check(!minigpu::is_inside_clip({0.0f, 0.0f, -1.001f, 1.0f}),
          "just outside -w");
}

void test_clip_rejects_invalid_w() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    check(!minigpu::is_inside_clip({0.0f, 0.0f, 0.0f, 0.0f}), "w = 0");
    check(!minigpu::is_inside_clip({0.0f, 0.0f, 0.0f, -1.0f}), "w < 0");
    check(!minigpu::is_inside_clip({nan, 0.0f, 0.0f, 1.0f}), "NaN x");
    check(!minigpu::is_inside_clip({0.0f, 0.0f, 0.0f, inf}), "infinite w");
    check(!minigpu::is_inside_clip({0.0f, 0.0f, 0.0f, -inf}),
          "negative infinite w");
}

void test_near_and_far_map_to_depth_edges() {
    const float near_plane = 1.0f;
    const float far_plane = 10.0f;
    const minigpu::Mat4 projection = minigpu::perspective(
        minigpu::degrees(90.0f), 1.0f, near_plane, far_plane);

    minigpu::Vec3 screen{};
    const minigpu::Vec4 near_clip =
        projection * minigpu::Vec4{0.0f, 0.0f, near_plane, 1.0f};
    check(minigpu::clip_to_screen(near_clip, 100, 100, screen),
          "near plane accepted");
    check_close(screen.z, 0.0f, "near maps to depth 0");

    const minigpu::Vec4 far_clip =
        projection * minigpu::Vec4{0.0f, 0.0f, far_plane, 1.0f};
    check(minigpu::clip_to_screen(far_clip, 100, 100, screen),
          "far plane accepted");
    check_close(screen.z, 1.0f, "far maps to depth 1");
}

void test_viewport_edges() {
    minigpu::Vec3 screen{};

    check(minigpu::clip_to_screen({-1.0f, -1.0f, -1.0f, 1.0f}, 100, 50,
                                  screen),
          "min corner accepted");
    check_close(screen.x, 0.0f, "min x");
    check_close(screen.y, 50.0f, "min y (origin top-left)");
    check_close(screen.z, 0.0f, "min depth");

    check(minigpu::clip_to_screen({1.0f, 1.0f, 1.0f, 1.0f}, 100, 50, screen),
          "max corner accepted");
    check_close(screen.x, 100.0f, "max x");
    check_close(screen.y, 0.0f, "max y (origin top-left)");
    check_close(screen.z, 1.0f, "max depth");
}

void test_tolerated_clip_boundaries_stay_in_viewport() {
    for (float edge : {-1.0000005f, 1.0000005f}) {
        for (const minigpu::Vec4 clip : {
                 minigpu::Vec4{edge, 0.0f, 0.0f, 1.0f},
                 minigpu::Vec4{0.0f, edge, 0.0f, 1.0f},
                 minigpu::Vec4{0.0f, 0.0f, edge, 1.0f}}) {
            minigpu::Vec3 screen{};
            check(minigpu::clip_to_screen(clip, 100, 50, screen),
                  "boundary within tolerance must be accepted");
            check(screen.x >= 0.0f && screen.x <= 100.0f &&
                      screen.y >= 0.0f && screen.y <= 50.0f &&
                      screen.z >= 0.0f && screen.z <= 1.0f,
                  "tolerated boundary must map inside viewport and depth range");
        }
    }

    minigpu::Vec3 screen{12.0f, 23.0f, 0.5f};
    check(!minigpu::clip_to_screen({0.0f, 0.0f, 1.001f, 1.0f}, 100, 50,
                                  screen),
          "boundary beyond tolerance must still be rejected");
    check(screen.x == 12.0f && screen.y == 23.0f && screen.z == 0.5f,
          "rejected boundary must leave output unchanged");
}

void test_perspective_parameter_guards() {
    check(throws_invalid_argument([] {
              minigpu::perspective(minigpu::degrees(0.0f), 1.0f, 1.0f, 10.0f);
          }),
          "fov = 0 must throw");
    check(throws_invalid_argument([] {
              minigpu::perspective(minigpu::degrees(180.0f), 1.0f, 1.0f, 10.0f);
          }),
          "fov = 180 must throw");
    check(throws_invalid_argument([] {
              minigpu::perspective(minigpu::degrees(60.0f), 0.0f, 1.0f, 10.0f);
          }),
          "aspect = 0 must throw");
    check(throws_invalid_argument([] {
              minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 0.0f, 10.0f);
          }),
          "near = 0 must throw");
    check(throws_invalid_argument([] {
              minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 10.0f, 10.0f);
          }),
          "near = far must throw");
    check(throws_invalid_argument([] {
              minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 10.0f, 5.0f);
          }),
          "near > far must throw");
}

void test_extreme_projection_is_finite() {
    const auto check_finite = [](const minigpu::Mat4& matrix,
                                 const char* what) {
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                if (!std::isfinite(matrix.m[column][row])) {
                    throw std::runtime_error(std::string(what) +
                                             " produced a non-finite matrix");
                }
            }
        }
    };

    check_finite(
        minigpu::perspective(minigpu::degrees(1.0f), 1e-3f, 0.1f, 100.0f),
        "narrow aspect");
    check_finite(
        minigpu::perspective(minigpu::degrees(179.0f), 1e3f, 0.1f, 100.0f),
        "wide aspect and wide fov");
    check_finite(
        minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 1e-4f, 100.0f),
        "tiny near plane");
    check_finite(
        minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 1.0f, 1.001f),
        "near close to far");
}

void test_large_coordinates_stay_finite() {
    const minigpu::Mat4 mvp =
        minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 0.1f, 1e7f) *
        minigpu::look_at({0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 0.0f},
                         {0.0f, 1.0f, 0.0f});
    const minigpu::Vec4 clip =
        mvp * minigpu::Vec4{1e6f, 1e6f, 1e6f, 1.0f};
    check(std::isfinite(clip.x) && std::isfinite(clip.y) &&
              std::isfinite(clip.z) && std::isfinite(clip.w),
          "large coordinates must stay finite");
}

void test_look_at_rejects_degenerate_inputs() {
    check(throws_invalid_argument([] {
              minigpu::look_at({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
                               {0.0f, 1.0f, 0.0f});
          }),
          "eye equal to target must throw");
    check(throws_invalid_argument([] {
              minigpu::look_at({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                               {0.0f, 0.0f, 2.0f});
          }),
          "up parallel to forward must throw");
}

void test_composition_matches_step_by_step() {
    const minigpu::Mat4 model =
        minigpu::translation({1.0f, 2.0f, 3.0f}) *
        minigpu::rotation_y(minigpu::degrees(30.0f)) *
        minigpu::scaling({1.5f, 1.1f, 0.7f});
    const minigpu::Mat4 view = minigpu::look_at(
        {3.0f, 2.0f, -6.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const minigpu::Mat4 projection =
        minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 0.1f, 100.0f);
    const minigpu::Vec4 point{0.5f, -0.25f, 1.0f, 1.0f};

    const minigpu::Vec4 combined = (projection * view * model) * point;
    const minigpu::Vec4 stepwise =
        projection * (view * (model * point));
    // 浮点乘法不满足严格结合律，这里只要求同一量级内的误差。
    check_close(combined, stepwise, "MVP composition equals step by step");
}

void test_transform_is_deterministic() {
    const auto run = [] {
        const minigpu::Mat4 mvp =
            minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 0.1f, 100.0f) *
            minigpu::look_at({3.0f, 2.0f, -6.0f}, {0.0f, 0.0f, 0.0f},
                             {0.0f, 1.0f, 0.0f}) *
            minigpu::rotation_y(minigpu::degrees(30.0f));
        return mvp * minigpu::Vec4{0.3f, -0.7f, 1.2f, 1.0f};
    };
    const minigpu::Vec4 first = run();
    const minigpu::Vec4 second = run();
    check(first.x == second.x && first.y == second.y &&
              first.z == second.z && first.w == second.w,
          "same input must produce bit-identical output");
}

void test_one_vertex_outside_rejects_triangle() {
    minigpu::Vec3 a{};
    minigpu::Vec3 b{};
    minigpu::Vec3 c{};
    check(minigpu::project_triangle({-1.0f, -1.0f, 0.0f, 1.0f},
                                    {1.0f, -1.0f, 0.0f, 1.0f},
                                    {0.0f, 1.0f, 0.0f, 1.0f}, 64, 64, a, b,
                                    c),
          "all inside passes");
    check(!minigpu::project_triangle({-1.0f, -1.0f, 0.0f, 1.0f},
                                     {1.5f, -1.0f, 0.0f, 1.0f},
                                     {0.0f, 1.0f, 0.0f, 1.0f}, 64, 64, a, b,
                                     c),
          "one vertex outside rejects the whole triangle");
}

}  // namespace

int main() {
    try {
        test_clip_boundary_is_inclusive();
        test_clip_rejects_invalid_w();
        test_near_and_far_map_to_depth_edges();
        test_viewport_edges();
        test_tolerated_clip_boundaries_stay_in_viewport();
        test_perspective_parameter_guards();
        test_extreme_projection_is_finite();
        test_large_coordinates_stay_finite();
        test_look_at_rejects_degenerate_inputs();
        test_composition_matches_step_by_step();
        test_transform_is_deterministic();
        test_one_vertex_outside_rejects_triangle();
    } catch (const std::exception& error) {
        std::cerr << "transform_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
