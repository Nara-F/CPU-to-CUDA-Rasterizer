#pragma once

#include "framebuffer.hpp"
#include "types.hpp"

namespace minigpu {

class CpuRasterizer {
   public:
    explicit CpuRasterizer(Framebuffer& target) : target_(target) {}
    // 顶点 z 须在 [0,1]；两种实现都线性插值屏幕深度，执行严格小于的深度测试。
    void draw_triangle(const Triangle& triangle);
    void draw_triangle_scanLine(const Triangle& triangle);
    void draw_clear(Color color) { target_.clear(color); }
    void export_ppm(const Framebuffer& fb, const std::filesystem::path& path);

   private:
    void shade_pixel(const Triangle& triangle, std::uint32_t x,
                     std::uint32_t y);
    Framebuffer& target_;
};

}  // namespace minigpu
