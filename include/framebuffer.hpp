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
    void clear(Color color);
    void set_pixel(std::uint32_t x, std::uint32_t y, Color color);
    void write_ppm(const std::filesystem::path& path) const;

   private:
    std::uint32_t width_, height_;
    std::vector<Color> pixels_;
};

}  // namespace minigpu
