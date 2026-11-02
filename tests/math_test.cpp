#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "minigpu.hpp"

namespace {

constexpr float kTolerance = 1e-6f;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}


std::array<float, 1> components(float value) { return {value}; }

std::array<float, 3> components(const minigpu::Vec3& v) {
    return {v.x, v.y, v.z};
}

std::array<float, 4> components(const minigpu::Vec4& v) {
    return {v.x, v.y, v.z, v.w};
}

std::array<float, 16> components(const minigpu::Mat4& m) {
    std::array<float, 16> parts{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row)
            parts[static_cast<std::size_t>(column) * 4 + row] =
                m.m[column][row];
    }
    return parts;
}

template <typename Value>
std::string component_label(std::size_t index) {
    if constexpr (std::is_same_v<Value, minigpu::Mat4>) {
        std::ostringstream where;
        where << " at [" << index / 4 << "][" << index % 4 << "]";
        return where.str();
    } else if constexpr (std::is_same_v<Value, float>) {
        (void)index;
        return {};
    } else {
        static constexpr const char* kNames[] = {"x", "y", "z", "w"};
        return std::string(" (") + kNames[index] + ")";
    }
}

template <typename Value>
void check_close(const Value& actual, const Value& expected,
                 const std::string& message) {
    const auto actual_parts = components(actual);
    const auto expected_parts = components(expected);
    for (std::size_t i = 0; i < actual_parts.size(); ++i) {
        if (std::fabs(actual_parts[i] - expected_parts[i]) <= kTolerance)
            continue;
        std::ostringstream error;
        error << message << component_label<Value>(i) << ": expected "
              << expected_parts[i] << ", got " << actual_parts[i];
        throw std::runtime_error(error.str());
    }
}

minigpu::Mat4 distinct_entries() {
    minigpu::Mat4 matrix;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row)
            matrix.m[column][row] = static_cast<float>(column * 10 + row);
    }
    return matrix;
}

void test_identity() {
    const minigpu::Mat4 identity;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            std::ostringstream where;
            where << "Mat4{} default at [" << column << "][" << row << "]";
            check_close(identity.m[column][row],
                        column == row ? 1.0f : 0.0f, where.str());
        }
    }

    check_close(identity * identity, identity, "identity * identity");

    const minigpu::Vec4 samples[] = {{0.0f, 0.0f, 0.0f, 1.0f},
                                     {1.0f, 2.0f, 3.0f, 1.0f},
                                     {-4.0f, 5.0f, -6.0f, 7.0f}};
    for (const minigpu::Vec4& v : samples)
        check_close(identity * v, v, "identity * v");
}

void test_translation() {
    const minigpu::Mat4 matrix = minigpu::translation({2.5f, -1.0f, 4.0f});
    check_close(matrix * minigpu::Vec4{1.0f, 1.0f, 1.0f, 1.0f},
                minigpu::Vec4{3.5f, 0.0f, 5.0f, 1.0f}, "translation moves a point");
    check_close(matrix * minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                minigpu::Vec4{2.5f, -1.0f, 4.0f, 1.0f}, "translation of the origin");
    check_close(matrix * minigpu::Vec4{1.0f, 0.0f, 0.0f, 0.0f},
                minigpu::Vec4{1.0f, 0.0f, 0.0f, 0.0f},
                "translation leaves directions alone");
}

void test_translation_composition() {
    const minigpu::Vec3 a = {1.0f, 2.0f, 3.0f};
    const minigpu::Vec3 b = {-4.0f, 0.5f, 2.0f};
    const minigpu::Vec3 sum = {a.x + b.x, a.y + b.y, a.z + b.z};
    check_close(minigpu::translation(a) * minigpu::translation(b),
                minigpu::translation(sum),
                "translations compose by adding their offsets");
}

void test_scaling() {
    check_close(minigpu::scaling({1.0f, 1.0f, 1.0f}), minigpu::Mat4{},
                "unit scaling is the identity");

    const minigpu::Mat4 scale = minigpu::scaling({2.0f, 3.0f, 4.0f});
    check_close(scale * minigpu::Vec4{1.0f, -2.0f, 3.0f, 1.0f},
                minigpu::Vec4{2.0f, -6.0f, 12.0f, 1.0f},
                "nonuniform scaling of a point");
    check_close(scale * minigpu::Vec4{1.0f, -2.0f, 3.0f, 0.0f},
                minigpu::Vec4{2.0f, -6.0f, 12.0f, 0.0f},
                "scaling changes directions and preserves w");
    check_close(scale * minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                "scaling is centered at the origin");
    check_close(minigpu::scaling({-2.0f, 0.0f, 3.0f}) *
                    minigpu::Vec4{1.0f, 2.0f, 3.0f, 1.0f},
                minigpu::Vec4{-2.0f, 0.0f, 9.0f, 1.0f},
                "negative and zero scales mirror and flatten axes");
}

void test_model_composition() {
    const minigpu::Mat4 model =
        minigpu::translation({5.0f, -1.0f, 8.0f}) *
        minigpu::rotation_y(minigpu::degrees(90.0f)) *
        minigpu::scaling({2.0f, 3.0f, 4.0f});

    // (1,2,1) -> S: (2,6,4) -> R: (4,6,-2) -> T: (9,5,6)。
    check_close(model * minigpu::Vec4{1.0f, 2.0f, 1.0f, 1.0f},
                minigpu::Vec4{9.0f, 5.0f, 6.0f, 1.0f},
                "T * R * S scales, rotates, then translates a point");
    check_close(model * minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                minigpu::Vec4{5.0f, -1.0f, 8.0f, 1.0f},
                "model translation is not scaled or rotated");
    check_close(model * minigpu::Vec4{1.0f, 2.0f, 1.0f, 0.0f},
                minigpu::Vec4{4.0f, 6.0f, -2.0f, 0.0f},
                "model directions receive scale and rotation only");
}

void test_column_major_layout() {
    const minigpu::Mat4 t = minigpu::translation({7.0f, 8.0f, 9.0f});
    check_close(t.m[3][0], 7.0f, "translation x lives at m[3][0]");
    check_close(t.m[3][1], 8.0f, "translation y lives at m[3][1]");
    check_close(t.m[3][2], 9.0f, "translation z lives at m[3][2]");
    check_close(t.m[3][3], 1.0f, "m[3][3] stays 1");
    check_close(t.m[0][3], 0.0f, "m[0][3] stays 0");
    check_close(t.m[1][3], 0.0f, "m[1][3] stays 0");
    check_close(t.m[2][3], 0.0f, "m[2][3] stays 0");
    check_close(distinct_entries() * minigpu::Vec4{1.0f, 2.0f, 3.0f, 4.0f},
                minigpu::Vec4{200.0f, 210.0f, 220.0f, 230.0f},
                "column-major M * v");
}

void test_matrix_product() {
    const minigpu::Mat4 m = distinct_entries();
    const minigpu::Mat4 identity;
    check_close(m * identity, m, "M * identity");
    check_close(identity * m, m, "identity * M");
    const minigpu::Vec4 v = {1.0f, 2.0f, 3.0f, 4.0f};
    const minigpu::Mat4 t = minigpu::translation({5.0f, 6.0f, 7.0f});
    check_close(t * (m * v), (t * m) * v, "(T * M) * v equals T * (M * v)");
}

void test_vector_operations() {
    check_close(minigpu::dot({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}), 0.0f,
                "dot of perpendicular axes");
    check_close(minigpu::dot({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}), 32.0f,
                "dot of general vectors");

    const minigpu::Vec3 x = {1.0f, 0.0f, 0.0f};
    const minigpu::Vec3 y = {0.0f, 1.0f, 0.0f};
    check_close(minigpu::cross(x, y), minigpu::Vec3{0.0f, 0.0f, 1.0f},
                "x cross y is +z");
    check_close(minigpu::cross(y, x), minigpu::Vec3{0.0f, 0.0f, -1.0f},
                "cross is antisymmetric");
    check_close(minigpu::cross({1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}),
                minigpu::Vec3{-3.0f, 6.0f, -3.0f}, "cross of general vectors");

    check_close(minigpu::length({3.0f, 4.0f, 0.0f}), 5.0f, "length");

    check_close(minigpu::normalize({0.0f, 0.0f, 5.0f}),
                minigpu::Vec3{0.0f, 0.0f, 1.0f}, "normalize keeps the direction");
    check_close(minigpu::length(minigpu::normalize({2.0f, -3.0f, 6.0f})), 1.0f,
                "normalize returns a unit vector");
}

void test_vector_arithmetic() {
    const minigpu::Vec3 a = {1.0f, 2.0f, 3.0f};
    const minigpu::Vec3 b = {4.0f, -5.0f, 6.0f};

    check_close(a + b, minigpu::Vec3{5.0f, -3.0f, 9.0f}, "vector addition");
    check_close(a - b, minigpu::Vec3{-3.0f, 7.0f, -3.0f}, "vector subtraction");
    check_close(-a, minigpu::Vec3{-1.0f, -2.0f, -3.0f}, "unary minus");
    check_close(a * 2.0f, minigpu::Vec3{2.0f, 4.0f, 6.0f},
                "vector times scalar");
    check_close(2.0f * a, minigpu::Vec3{2.0f, 4.0f, 6.0f},
                "scalar times vector");
    check_close((a + b) - b, a, "addition and subtraction cancel");
}

void test_normalize_rejects_zero_length() {
    bool thrown = false;
    try {
        (void)minigpu::normalize({0.0f, 0.0f, 0.0f});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "normalize of the zero vector must throw std::invalid_argument");
}

void test_look_at() {
    // 相机在原点朝 +Z、up = +Y：视图矩阵就是单位矩阵。
    check_close(minigpu::look_at({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                                 {0.0f, 1.0f, 0.0f}),
                minigpu::Mat4{}, "look_at from origin along +Z");

    // 相机在 (0,0,-5) 看原点：原点在相机前方 5 单位，相机位置映到原点。
    const minigpu::Mat4 view = minigpu::look_at(
        {0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    check_close(view * minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                minigpu::Vec4{0.0f, 0.0f, 5.0f, 1.0f},
                "origin is 5 units in front of the camera");
    check_close(view * minigpu::Vec4{0.0f, 0.0f, -5.0f, 1.0f},
                minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                "eye maps to the view-space origin");

    // 相机在 (0,0,-5) 朝 +X 看：只测"朝 +Z"抓不到基向量的手性错误。
    const minigpu::Mat4 view_x = minigpu::look_at(
        {0.0f, 0.0f, -5.0f}, {5.0f, 0.0f, -5.0f}, {0.0f, 1.0f, 0.0f});
    check_close(view_x * minigpu::Vec4{5.0f, 0.0f, -5.0f, 1.0f},
                minigpu::Vec4{0.0f, 0.0f, 5.0f, 1.0f},
                "target is 5 units in front when looking along +X");
    // 相机右侧是 -Z，所以世界原点在相机的左侧 5 单位。
    check_close(view_x * minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                minigpu::Vec4{-5.0f, 0.0f, 0.0f, 1.0f},
                "world +Z lies to the camera's left");
}

void test_perspective() {
    const float near_plane = 1.0f;
    const float far_plane = 10.0f;
    const minigpu::Mat4 projection =
        minigpu::perspective(minigpu::degrees(90.0f), 1.0f, near_plane,
                             far_plane);

    // w_clip = z_view，所以 NDC = clip / z_view。
    const minigpu::Vec4 near_clip =
        projection * minigpu::Vec4{0.0f, 0.0f, near_plane, 1.0f};
    check_close(near_clip.w, near_plane, "w_clip equals z_view");
    check_close(near_clip.z / near_clip.w, -1.0f, "near maps to NDC z = -1");

    const minigpu::Vec4 far_clip =
        projection * minigpu::Vec4{0.0f, 0.0f, far_plane, 1.0f};
    check_close(far_clip.z / far_clip.w, 1.0f, "far maps to NDC z = +1");

    // 90 度 FOV、aspect = 1 时，z 处的可视半宽等于 z。
    const minigpu::Vec4 edge =
        projection * minigpu::Vec4{1.0f, 0.0f, 1.0f, 1.0f};
    check_close(edge.x / edge.w, 1.0f, "90 degree FOV maps x = z to NDC x = 1");

    // aspect = 2：同样的 x 映射到 NDC 的一半，可视宽度翻倍。
    const minigpu::Mat4 wide =
        minigpu::perspective(minigpu::degrees(90.0f), 2.0f, near_plane,
                             far_plane);
    const minigpu::Vec4 wide_edge =
        wide * minigpu::Vec4{2.0f, 0.0f, 1.0f, 1.0f};
    check_close(wide_edge.x / wide_edge.w, 1.0f,
                "aspect scales the visible width");
}

void test_rotation_x() {
    check_close(minigpu::rotation_x(minigpu::degrees(0.0f)), minigpu::Mat4{},
                "X rotation by zero is the identity");

    const minigpu::Vec4 point{2.0f, 3.0f, 4.0f, 1.0f};
    check_close(minigpu::rotation_x(minigpu::degrees(90.0f)) * point,
                minigpu::Vec4{2.0f, -4.0f, 3.0f, 1.0f},
                "X rotation by +90 preserves x and sends +Y to +Z");
    check_close(minigpu::rotation_x(minigpu::degrees(-90.0f)) * point,
                minigpu::Vec4{2.0f, 4.0f, -3.0f, 1.0f},
                "X rotation by -90 reverses the rotation");
}

void test_rotation_y() {
    check_close(minigpu::rotation_y(minigpu::degrees(0.0f)), minigpu::Mat4{},
                "rotation by zero is the identity");

    check_close(minigpu::rotation_y(minigpu::degrees(90.0f)) *
                    minigpu::Vec4{1.0f, 0.0f, 0.0f, 1.0f},
                minigpu::Vec4{0.0f, 0.0f, -1.0f, 1.0f},
                "90 degrees sends +X to -Z");
}

void test_rotation_z() {
    check_close(minigpu::rotation_z(minigpu::degrees(0.0f)), minigpu::Mat4{},
                "Z rotation by zero is the identity");

    const minigpu::Vec4 point{2.0f, 3.0f, 4.0f, 1.0f};
    check_close(minigpu::rotation_z(minigpu::degrees(90.0f)) * point,
                minigpu::Vec4{-3.0f, 2.0f, 4.0f, 1.0f},
                "Z rotation by +90 preserves z and sends +X to +Y");
    check_close(minigpu::rotation_z(minigpu::degrees(-90.0f)) * point,
                minigpu::Vec4{3.0f, -2.0f, 4.0f, 1.0f},
                "Z rotation by -90 reverses the rotation");
}

void test_rotation_properties() {
    using Rotation = minigpu::Mat4 (*)(minigpu::Radians);
    const Rotation rotations[] = {minigpu::rotation_x, minigpu::rotation_y,
                                  minigpu::rotation_z};
    for (const auto rotation : rotations) {
        const auto matrix = rotation(minigpu::degrees(37.0f));
        check_close(matrix * rotation(minigpu::degrees(-37.0f)),
                    minigpu::Mat4{}, "opposite rotations cancel");
        check_close(matrix * minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                    minigpu::Vec4{0.0f, 0.0f, 0.0f, 1.0f},
                    "rotation leaves the origin fixed");

        for (const float w : {0.0f, 1.0f}) {
            const auto rotated = matrix * minigpu::Vec4{1.0f, 2.0f, 2.0f, w};
            check_close(rotated.w, w, "rotation preserves homogeneous w");
            check_close(minigpu::length({rotated.x, rotated.y, rotated.z}),
                        3.0f, "rotation preserves vector length");
        }
    }
}

void test_rotation_composition() {
    const auto x = minigpu::rotation_x(minigpu::degrees(90.0f));
    const auto z = minigpu::rotation_z(minigpu::degrees(90.0f));
    const minigpu::Vec4 y_axis{0.0f, 1.0f, 0.0f, 0.0f};
    check_close((z * x) * y_axis, minigpu::Vec4{0.0f, 0.0f, 1.0f, 0.0f},
                "Rz * Rx applies X rotation before Z rotation");
    check_close((x * z) * y_axis, minigpu::Vec4{-1.0f, 0.0f, 0.0f, 0.0f},
                "swapping X and Z rotations changes the result");
}

void test_angle_units() {
    check_close(minigpu::degrees(180.0f).value, minigpu::kPi,
                "180 degrees equals pi");
    check_close(minigpu::degrees(90.0f).value, minigpu::kPi * 0.5f,
                "90 degrees equals pi/2");
    check_close(minigpu::radians(1.5f).value, 1.5f, "radians passes through");
}

}  // namespace

int main() {
    try {
        test_identity();
        test_look_at();
        test_perspective();
        test_rotation_x();
        test_rotation_y();
        test_rotation_z();
        test_rotation_properties();
        test_rotation_composition();
        test_angle_units();
        test_translation();
        test_translation_composition();
        test_scaling();
        test_model_composition();
        test_column_major_layout();
        test_matrix_product();
        test_vector_operations();
        test_vector_arithmetic();
        test_normalize_rejects_zero_length();
    } catch (const std::exception& error) {
        std::cerr << "math_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
