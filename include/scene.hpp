#pragma once

#include <cstdint>
#include <vector>

#include "types.hpp"

namespace minigpu {

// Lab03 硬编码演示入口：顶点、索引和面颜色均内置于函数实现。
// cube 局部坐标各轴为 [-1,1]，8 个顶点、12 个索引三角形，按面配色。
// Lab04 的 OBJ 加载及通用网格变换入口在对应 lab 实现。
// mvp = P * V * M；返回经裁剪拒绝、透视除法和 viewport 映射的屏幕三角形，
// 保留原始索引顺序。不做背面剔除，由深度测试处理遮挡。
// width / height 是非零像素尺寸，否则抛 std::invalid_argument。
std::vector<Triangle> make_cube_triangles_hardcoded(const Mat4& mvp,
                                                   std::uint32_t width,
                                                   std::uint32_t height);

}  // namespace minigpu
