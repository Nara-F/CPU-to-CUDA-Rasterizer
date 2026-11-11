#include "scene.hpp"

#include <array>
#include <stdexcept>

#include "raster_math.hpp"

namespace minigpu {

std::vector<Triangle> make_cube_triangles_hardcoded(const Mat4& mvp,
                                                   std::uint32_t width,
                                                   std::uint32_t height) {
    if (width == 0 || height == 0)
        throw std::invalid_argument("scene dimensions must be non-zero");

    constexpr std::array<Vec4, 8> vertices{{
        {-1, -1, -1, 1}, {1, -1, -1, 1}, {1, 1, -1, 1}, {-1, 1, -1, 1},
        {-1, -1, 1, 1},  {1, -1, 1, 1},  {1, 1, 1, 1},  {-1, 1, 1, 1},
    }};
    // 每两个三角形组成一个面；从 cube 外侧看，各面保持相同绕序。
    constexpr std::array<std::array<unsigned, 3>, 12> indices{{
        {{0, 2, 1}}, {{0, 3, 2}},  // -Z
        {{4, 5, 6}}, {{4, 6, 7}},  // +Z
        {{0, 4, 7}}, {{0, 7, 3}},  // -X
        {{1, 2, 6}}, {{1, 6, 5}},  // +X
        {{0, 1, 5}}, {{0, 5, 4}},  // -Y
        {{3, 7, 6}}, {{3, 6, 2}},  // +Y
    }};
    constexpr std::array<Color, 6> colors{{
        {230, 75, 65, 255}, {65, 190, 110, 255}, {70, 120, 230, 255},
        {240, 175, 55, 255}, {170, 85, 210, 255}, {65, 200, 215, 255},
    }};

    std::array<Vec4, 8> clip;
    for (std::size_t i = 0; i < vertices.size(); ++i)
        clip[i] = mvp * vertices[i];

    std::vector<Triangle> triangles;
    triangles.reserve(indices.size());
    for (std::size_t i = 0; i < indices.size(); ++i) {
        const auto& index = indices[i];
        Vec3 a, b, c;
        if (project_triangle(clip[index[0]], clip[index[1]], clip[index[2]],
                              width, height, a, b, c)) {
            const Color color = colors[i / 2];
            triangles.push_back({{a, color}, {b, color}, {c, color}});
        }
    }
    return triangles;
}

}  // namespace minigpu
