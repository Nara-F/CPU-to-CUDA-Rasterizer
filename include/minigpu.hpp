#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace minigpu {

struct Color {
  std::uint8_t r = 0, g = 0, b = 0, a = 255;
};

struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec4 { float x = 0, y = 0, z = 0, w = 1; };
// Lab contract: positions are screen coordinates; z is normalized depth in [0, 1].
struct ScreenVertex { Vec3 position; Color color{}; };
struct Triangle { ScreenVertex a, b, c; };

class Framebuffer {
 public:
  Framebuffer(std::uint32_t width, std::uint32_t height, Color clear = {});
  std::uint32_t width() const noexcept { return width_; }
  std::uint32_t height() const noexcept { return height_; }
  Color& at(std::uint32_t x, std::uint32_t y);
  const Color& at(std::uint32_t x, std::uint32_t y) const;
  void clear(Color color);
  void write_ppm(const std::filesystem::path& path) const;

 private:
  std::uint32_t width_, height_;
  std::vector<Color> pixels_;
};

class CpuRasterizer {
 public:
  explicit CpuRasterizer(Framebuffer& target) : target_(target) {}
  void draw_triangle(const Triangle& triangle);
  void draw_clear(Color color) { target_.clear(color); }

 private:
  Framebuffer& target_;
};

void run_cuda_demo(Framebuffer& target);

}  // namespace minigpu
