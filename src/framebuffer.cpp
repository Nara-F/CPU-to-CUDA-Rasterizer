#include "framebuffer.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace minigpu {

Framebuffer::Framebuffer(std::uint32_t width, std::uint32_t height, Color clear)
    : width_(width), height_(height) {
    if (width == 0 || height == 0)
        throw std::invalid_argument("framebuffer dimensions must be non-zero");
    if (height > std::numeric_limits<std::size_t>::max() / width ||
        static_cast<std::size_t>(width) * height >
            std::vector<Color>().max_size() ||
        static_cast<std::size_t>(width) * height >
            std::vector<float>().max_size())
        throw std::length_error("framebuffer dimensions are too large");
    pixels_.assign(static_cast<std::size_t>(width) * height, clear);
    depths_.assign(pixels_.size(), std::numeric_limits<float>::infinity());
}

Color& Framebuffer::at(std::uint32_t x, std::uint32_t y) {
    if (x >= width_ || y >= height_)
        throw std::out_of_range("framebuffer coordinate out of range");
    return pixels_[static_cast<std::size_t>(y) * width_ + x];
}

const Color& Framebuffer::at(std::uint32_t x, std::uint32_t y) const {
    return const_cast<Framebuffer*>(this)->at(x, y);
}

float Framebuffer::depth_at(std::uint32_t x, std::uint32_t y) const {
    if (x >= width_ || y >= height_)
        throw std::out_of_range("framebuffer coordinate out of range");
    return depths_[static_cast<std::size_t>(y) * width_ + x];
}

void Framebuffer::clear(Color color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
    std::fill(depths_.begin(), depths_.end(),
              std::numeric_limits<float>::infinity());
}

void Framebuffer::set_pixel(std::uint32_t x, std::uint32_t y, Color color) {
    if (x >= width_ || y >= height_)
        return;  // Ignore out-of-bounds pixels instead of throwing an exception
    at(x, y) = color;
}

bool Framebuffer::depth_test_and_set_pixel(std::uint32_t x, std::uint32_t y,
                                          float depth, Color color) {
    if (x >= width_ || y >= height_ || !std::isfinite(depth) ||
        depth < 0.0f || depth > 1.0f)
        return false;
    const std::size_t index = static_cast<std::size_t>(y) * width_ + x;
    if (!(depth < depths_[index])) return false;
    pixels_[index] = color;
    depths_[index] = depth;
    return true;
}

void Framebuffer::write_ppm(const std::filesystem::path& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw std::runtime_error("cannot open PPM output: " + path.string());
    out << "P6\n" << width_ << ' ' << height_ << "\n255\n";
    for (const Color pixel : pixels_) {
        const char bytes[3] = {static_cast<char>(pixel.r),
                               static_cast<char>(pixel.g),
                               static_cast<char>(pixel.b)};
        out.write(bytes, 3);
    }
    out.flush();
    if (!out)
        throw std::runtime_error("failed while writing PPM output: " +
                                 path.string());
}

}  // namespace minigpu
