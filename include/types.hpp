#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "math.hpp"

namespace minigpu {

struct Color {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
};

using TriangleIndices = std::array<std::uint32_t, 3>;

// 网格输入：OBJ loader 或场景构造器的输出。
// colors 为空表示没有顶点颜色，构建时按三角形序号确定性配色；
// 非空时必须与 positions 等长。每组 indices 表示一个三角形，
// 三个索引均为 0 基，且必须在 positions 范围内。
struct Mesh {
    std::vector<Vec3> positions;
    std::vector<Color> colors;
    std::vector<TriangleIndices> indices;
};

// Screen-space contract: floating-point x/y, origin at the top left,
// +X right and +Y down; pixel (x,y) is sampled at (x+0.5, y+0.5).
// Viewport: x = (ndc_x + 1) * width / 2,
//           y = (1 - ndc_y) * height / 2.
// z = (ndc_z + 1) / 2 is depth in [0,1]: near = 0, far = 1.
// Smaller depth wins; equal depth keeps the first input triangle.
// Color channels are RGBA bytes in [0,255].
struct ScreenVertex {
    Vec3 position;
    Color color{};
};
struct Triangle {
    ScreenVertex a, b, c;
};

}  // namespace minigpu
