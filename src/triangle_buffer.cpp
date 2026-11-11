#include "triangle_buffer.hpp"

#include <array>
#include <stdexcept>

#include "raster_math.hpp"

namespace minigpu {

namespace {

// 无顶点颜色时的确定性配色，按三角形序号循环。
constexpr std::array<Color, 6> kPalette{{
    {230, 75, 65, 255},  {65, 190, 110, 255}, {70, 120, 230, 255},
    {240, 175, 55, 255}, {170, 85, 210, 255}, {65, 200, 215, 255},
}};

void validate_mesh(const Mesh& mesh) {
    if (mesh.indices.size() % 3 != 0) {
        throw std::invalid_argument(
            "mesh index count must be a multiple of 3");
    }
    if (!mesh.colors.empty() && mesh.colors.size() != mesh.positions.size()) {
        throw std::invalid_argument(
            "mesh color count must match position count");
    }
    for (const std::uint32_t index : mesh.indices) {
        if (index >= mesh.positions.size()) {
            throw std::invalid_argument("mesh index out of range");
        }
    }
}

}  // namespace

TriangleBuffer build_screen_triangles(const Mat4& mvp, const Mesh& mesh,
                                      std::uint32_t width,
                                      std::uint32_t height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("buffer dimensions must be non-zero");
    }
    validate_mesh(mesh);

    // 每个顶点只变换一次；CPU 与 CUDA 复用这份结果。
    const std::size_t vertex_count = mesh.positions.size();
    std::vector<Vec3> screen(vertex_count);
    std::vector<bool> accepted(vertex_count, false);
    for (std::size_t i = 0; i < vertex_count; ++i) {
        const Vec3& p = mesh.positions[i];
        const Vec4 clip = mvp * Vec4{p.x, p.y, p.z, 1.0f};
        accepted[i] = clip_to_screen(clip, width, height, screen[i]);
    }

    TriangleBuffer buffer;
    buffer.vertices.reserve(mesh.indices.size());
    for (std::size_t triangle = 0; triangle < mesh.indices.size() / 3;
         ++triangle) {
        const std::uint32_t ia = mesh.indices[triangle * 3 + 0];
        const std::uint32_t ib = mesh.indices[triangle * 3 + 1];
        const std::uint32_t ic = mesh.indices[triangle * 3 + 2];

        // 契约：任一顶点越界则整三角形丢弃。
        if (!accepted[ia] || !accepted[ib] || !accepted[ic]) continue;

        // 无顶点颜色时按三角形序号平面着色，保证确定性。
        const Color flat = kPalette[triangle % kPalette.size()];
        const Color ca = mesh.colors.empty() ? flat : mesh.colors[ia];
        const Color cb = mesh.colors.empty() ? flat : mesh.colors[ib];
        const Color cc = mesh.colors.empty() ? flat : mesh.colors[ic];

        buffer.vertices.push_back({screen[ia], ca});
        buffer.vertices.push_back({screen[ib], cb});
        buffer.vertices.push_back({screen[ic], cc});
    }
    return buffer;
}

}  // namespace minigpu
