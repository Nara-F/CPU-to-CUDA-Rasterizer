#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "math.hpp"
#include "types.hpp"

namespace minigpu {

// 屏幕空间三角形缓冲：连续、POD、保序，可直接 memcpy / cudaMemcpy 到设备。
// vertices 每 3 个构成一个三角形；每个顶点已含屏幕 x/y、归一化深度和 RGBA，
// 设备端不需要矩阵，只负责光栅化。
struct TriangleBuffer {
    std::vector<ScreenVertex> vertices;

    std::size_t triangle_count() const noexcept { return vertices.size() / 3; }
    bool empty() const noexcept { return vertices.empty(); }
    const ScreenVertex* triangle(std::size_t index) const noexcept {
        return vertices.data() + index * 3;
    }
    ScreenVertex* triangle(std::size_t index) noexcept {
        return vertices.data() + index * 3;
    }
};

// 上传布局契约：固定 16 字节 / 顶点，四个标量字段，无填充差异。
static_assert(sizeof(ScreenVertex) == 16,
              "ScreenVertex must stay 16 bytes for device upload");
static_assert(std::is_trivially_copyable<ScreenVertex>::value,
              "ScreenVertex must be trivially copyable");
static_assert(sizeof(ScreenVertex) == 3 * sizeof(float) + sizeof(Color),
              "ScreenVertex must have no padding");

// 主机端一次完成 P*V*M、有限值与 w > 0 检查、裁剪拒绝、透视除法和 viewport，
// 输出屏幕空间的连续三角形缓冲，并保持输入三角形顺序（等深判定依赖顺序）。
// 任一顶点越界则整三角形丢弃。
// width / height 必须非零；网格非法（颜色数量不匹配、索引越界）
// 抛 std::invalid_argument。
TriangleBuffer build_screen_triangles(const Mat4& mvp, const Mesh& mesh,
                                      std::uint32_t width,
                                      std::uint32_t height);

}  // namespace minigpu
