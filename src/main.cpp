#include "minigpu.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void usage(const char* name) {
  std::cerr << "Usage: " << name << " [--help] [--output PATH] [--width N] [--height N] [--backend starter|cpu|cuda-naive|cuda-tiled]\n";
}

std::uint32_t parse_dimension(const std::string& text, const char* option) {
  if (text.empty() || text.front() == '-' || text.find_first_not_of("0123456789") != std::string::npos)
    throw std::invalid_argument(std::string("invalid positive integer for ") + option);
  std::size_t consumed = 0;
  const unsigned long long value = std::stoull(text, &consumed);
  if (consumed != text.size() || value == 0 || value > std::numeric_limits<std::uint32_t>::max())
    throw std::invalid_argument(std::string("invalid positive integer for ") + option);
  return static_cast<std::uint32_t>(value);
}
}

int main(int argc, char** argv) {
  std::filesystem::path output = "minigpu.ppm";
  std::uint32_t width = 256, height = 256;
  std::string backend = "starter";
  try {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      auto value = [&](const char* option) -> std::string {
        if (i + 1 >= argc) throw std::invalid_argument(std::string("missing value for ") + option);
        return argv[++i];
      };
      if (arg == "--help") { usage(argv[0]); return 0; }
      else if (arg == "--output") output = value("--output");
      else if (arg == "--width") width = parse_dimension(value("--width"), "--width");
      else if (arg == "--height") height = parse_dimension(value("--height"), "--height");
      else if (arg == "--backend") backend = value("--backend");
      else { usage(argv[0]); return 2; }
    }
    minigpu::Framebuffer framebuffer(width, height, {24, 32, 48, 255});
    if (backend == "starter") {
      minigpu::CpuRasterizer rasterizer(framebuffer);
      rasterizer.draw_clear({24, 32, 48, 255});
    } else if (backend == "cpu") {
      minigpu::CpuRasterizer rasterizer(framebuffer);
      rasterizer.draw_triangle({});
    } else if (backend == "cuda-naive") {
      minigpu::run_cuda_demo(framebuffer);
    } else if (backend == "cuda-tiled") {
      throw std::logic_error("tiled CUDA backend is a lab TODO (Weeks 9-10)");
    } else { throw std::invalid_argument("unknown backend: " + backend); }
    framebuffer.write_ppm(output);
    std::cout << "Wrote " << output << " (" << width << "x" << height << ", " << backend << " starter)\n";
  } catch (const std::exception& error) {
    std::cerr << "minigpu: " << error.what() << '\n';
    return 1;
  }
}
