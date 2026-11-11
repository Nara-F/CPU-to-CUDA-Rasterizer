#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

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

void check_close(const minigpu::Vec3& actual, const minigpu::Vec3& expected,
                 const std::string& message) {
    check_close(actual.x, expected.x, message + " (x)");
    check_close(actual.y, expected.y, message + " (y)");
    check_close(actual.z, expected.z, message + " (z)");
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

minigpu::Mesh unit_triangle_mesh() {
    minigpu::Mesh mesh;
    mesh.positions = {
        {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}, {0.0f, 0.5f, 0.0f}};
    mesh.indices = {0, 1, 2};
    return mesh;
}

void test_upload_layout() {
    static_assert(std::is_trivially_copyable<minigpu::ScreenVertex>::value,
                  "ScreenVertex must be trivially copyable");
    check(sizeof(minigpu::Color) == 4, "Color must be 4 bytes");
    check(sizeof(minigpu::ScreenVertex) == 16, "ScreenVertex must be 16 bytes");
    check(sizeof(minigpu::Triangle) == 48, "Triangle must be 48 bytes");
    check(sizeof(minigpu::TriangleBuffer::vertices[0]) ==
              sizeof(minigpu::ScreenVertex),
          "buffer element is ScreenVertex");
}

void test_builds_contiguous_triangles() {
    const minigpu::TriangleBuffer buffer = minigpu::build_screen_triangles(
        minigpu::Mat4{}, unit_triangle_mesh(), 100, 100);

    check(buffer.triangle_count() == 1, "one triangle");
    check(buffer.vertices.size() == 3, "three vertices");
    check(&buffer.vertices[0] + 3 == buffer.triangle(1),
          "triangles are contiguous");

    // 单位矩阵 + NDC 坐标：x = 25, y = 75（原点左上）。
    check_close(buffer.vertices[0].position,
                minigpu::Vec3{25.0f, 75.0f, 0.5f},
                "viewport mapping of the first vertex");
}

void test_preserves_order_and_flat_color() {
    minigpu::Mesh mesh;
    mesh.positions = {
        {-0.5f, -0.5f, 0.0f}, {0.0f, -0.5f, 0.0f}, {-0.25f, 0.0f, 0.0f},
        {0.25f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}, {0.375f, 0.0f, 0.0f}};
    mesh.indices = {0, 1, 2, 3, 4, 5};

    const minigpu::TriangleBuffer buffer = minigpu::build_screen_triangles(
        minigpu::Mat4{}, mesh, 100, 100);
    check(buffer.triangle_count() == 2, "two triangles kept");

    // 第一个三角形仍然排在前面：positions[0] = (-0.5,-0.5) → x = 25。
    check_close(buffer.vertices[0].position.x, 25.0f,
                "input order preserved");
    check_close(buffer.vertices[3].position.x, 62.5f,
                "second triangle follows the first");

    // 无顶点颜色时按三角形序号配色：同一三角形三顶点同色，不同三角形不同色。
    check(buffer.vertices[0].color.r == buffer.vertices[1].color.r &&
              buffer.vertices[0].color.g == buffer.vertices[1].color.g &&
              buffer.vertices[0].color.b == buffer.vertices[1].color.b,
          "flat shading per triangle");
    check(!(buffer.vertices[0].color.r == buffer.vertices[3].color.r &&
            buffer.vertices[0].color.g == buffer.vertices[3].color.g &&
            buffer.vertices[0].color.b == buffer.vertices[3].color.b),
          "palette advances with triangle index");
}

void test_vertex_colors_are_preserved() {
    minigpu::Mesh mesh = unit_triangle_mesh();
    mesh.colors = {{10, 20, 30, 255}, {40, 50, 60, 255}, {70, 80, 90, 255}};

    const minigpu::TriangleBuffer buffer = minigpu::build_screen_triangles(
        minigpu::Mat4{}, mesh, 100, 100);
    check(buffer.triangle_count() == 1, "triangle kept");
    check(buffer.vertices[0].color.r == 10 && buffer.vertices[1].color.r == 40 &&
              buffer.vertices[2].color.r == 70,
          "per-vertex colors preserved");
}

void test_drops_whole_triangle_when_one_vertex_is_outside() {
    minigpu::Mesh mesh;
    mesh.positions = {
        {-0.5f, -0.5f, 0.0f}, {1.5f, -0.5f, 0.0f}, {0.0f, 0.5f, 0.0f}};
    mesh.indices = {0, 1, 2};

    const minigpu::TriangleBuffer buffer = minigpu::build_screen_triangles(
        minigpu::Mat4{}, mesh, 100, 100);
    check(buffer.empty(), "one outside vertex drops the whole triangle");
}

void test_rejects_invalid_mesh() {
    minigpu::Mesh bad_count = unit_triangle_mesh();
    bad_count.indices = {0, 1};
    check(throws_invalid_argument([&] {
              minigpu::build_screen_triangles(minigpu::Mat4{}, bad_count, 10,
                                              10);
          }),
          "index count must be a multiple of 3");

    minigpu::Mesh out_of_range = unit_triangle_mesh();
    out_of_range.indices = {0, 1, 3};
    check(throws_invalid_argument([&] {
              minigpu::build_screen_triangles(minigpu::Mat4{}, out_of_range,
                                              10, 10);
          }),
          "index out of range must throw");

    minigpu::Mesh bad_colors = unit_triangle_mesh();
    bad_colors.colors = {{1, 2, 3, 255}};
    check(throws_invalid_argument([&] {
              minigpu::build_screen_triangles(minigpu::Mat4{}, bad_colors, 10,
                                              10);
          }),
          "color count mismatch must throw");

    check(throws_invalid_argument([&] {
              minigpu::build_screen_triangles(minigpu::Mat4{},
                                              unit_triangle_mesh(), 0, 10);
          }),
          "zero width must throw");
}

void test_memcpy_round_trip() {
    minigpu::Mesh mesh = unit_triangle_mesh();
    const minigpu::TriangleBuffer buffer = minigpu::build_screen_triangles(
        minigpu::Mat4{}, mesh, 100, 100);
    check(!buffer.empty(), "buffer is not empty");

    const std::size_t bytes =
        buffer.vertices.size() * sizeof(minigpu::ScreenVertex);
    std::vector<unsigned char> raw(bytes);
    std::memcpy(raw.data(), buffer.vertices.data(), bytes);

    std::vector<minigpu::ScreenVertex> restored(buffer.vertices.size());
    std::memcpy(restored.data(), raw.data(), bytes);

    check(std::memcmp(buffer.vertices.data(), restored.data(), bytes) == 0,
          "device-style upload must be byte-identical");
}

// 新的缓冲构建路径必须与既有硬编码 cube 路径产生相同的屏幕三角形。
void test_matches_hardcoded_cube() {
    minigpu::Mesh mesh;
    mesh.positions = {{-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
                      {1.0f, 1.0f, -1.0f},   {-1.0f, 1.0f, -1.0f},
                      {-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f},
                      {1.0f, 1.0f, 1.0f},    {-1.0f, 1.0f, 1.0f}};
    mesh.indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 4, 7, 0, 7,
                    3, 1, 2, 6, 1, 6, 5, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3,
                    6, 2};

    const minigpu::Mat4 projection =
        minigpu::perspective(minigpu::degrees(60.0f), 1.0f, 0.1f, 100.0f);
    const minigpu::Mat4 view = minigpu::look_at(
        {3.0f, 2.0f, -6.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const minigpu::Mat4 model = minigpu::rotation_y(minigpu::degrees(30.0f));
    const minigpu::Mat4 mvp = projection * view * model;

    const minigpu::TriangleBuffer built =
        minigpu::build_screen_triangles(mvp, mesh, 128, 128);
    const std::vector<minigpu::Triangle> reference =
        minigpu::make_cube_triangles_hardcoded(mvp, 128, 128);

    check(built.triangle_count() == reference.size(),
          "buffer and hardcoded scene must keep the same triangles");
    for (std::size_t i = 0; i < reference.size(); ++i) {
        const minigpu::ScreenVertex* triangle = built.triangle(i);
        check_close(triangle[0].position, reference[i].a.position,
                    "vertex a");
        check_close(triangle[1].position, reference[i].b.position,
                    "vertex b");
        check_close(triangle[2].position, reference[i].c.position,
                    "vertex c");
    }
}

}  // namespace

int main() {
    try {
        test_upload_layout();
        test_builds_contiguous_triangles();
        test_preserves_order_and_flat_color();
        test_vertex_colors_are_preserved();
        test_drops_whole_triangle_when_one_vertex_is_outside();
        test_rejects_invalid_mesh();
        test_memcpy_round_trip();
        test_matches_hardcoded_cube();
    } catch (const std::exception& error) {
        std::cerr << "triangle_buffer_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
