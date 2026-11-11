#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "types.hpp"

namespace minigpu {

class Framebuffer {
   public:
    Framebuffer(std::uint32_t width, std::uint32_t height, Color clear = {});
    std::uint32_t width() const noexcept { return width_; }
    std::uint32_t height() const noexcept { return height_; }
    Color& at(std::uint32_t x, std::uint32_t y);
    const Color& at(std::uint32_t x, std::uint32_t y) const;
    // 深度初值为正无穷；坐标越界与 at() 一样抛 std::out_of_range。
    float depth_at(std::uint32_t x, std::uint32_t y) const;
    // 同时清空颜色与深度。直接 at()/set_pixel() 写颜色不修改深度。
    void clear(Color color);
    void set_pixel(std::uint32_t x, std::uint32_t y, Color color);
    // depth 为 [0,1] 中的有限值；严格小于旧深度时一起写入颜色和深度。
    // 等深保留先写入者；越界或非法深度返回 false，不修改缓冲。
    bool depth_test_and_set_pixel(std::uint32_t x, std::uint32_t y,
                                  float depth, Color color);
    void write_ppm(const std::filesystem::path& path) const;

   private:
    std::uint32_t width_, height_;
    std::vector<Color> pixels_;
    std::vector<float> depths_;
};

}  // namespace minigpu
