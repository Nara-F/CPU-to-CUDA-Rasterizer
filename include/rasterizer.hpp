#pragma once

#include "framebuffer.hpp"
#include "types.hpp"

namespace minigpu {

class CpuRasterizer {
   public:
    explicit CpuRasterizer(Framebuffer& target) : target_(target) {}
    void draw_triangle(const Triangle& triangle);
    void draw_triangle_scanLine(const Triangle& triangle);
    void draw_clear(Color color) { target_.clear(color); }
    void export_ppm(const Framebuffer& fb, const std::filesystem::path& path);

   private:
    Framebuffer& target_;
};

}  // namespace minigpu
