#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "minigpu.hpp"

namespace {
void usage(const char* name) {
    std::cerr
        << "Usage: " << name
        << " [--help] [--output PATH] [--width N] [--height N] "
           "[--backend starter|cpu|cuda-naive|cuda-tiled] "
           "[--scene cube|triangles] [--angle DEGREES] [--reverse-order]\n"
        << "CPU defaults to cube at 30 degrees; --angle and "
           "--reverse-order apply to cube.\n";
}

std::uint32_t parse_dimension(const std::string& text, const char* option) {
    if (text.empty() || text.front() == '-' ||
        text.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument(
            std::string("invalid positive integer for ") + option);
    std::size_t consumed = 0;
    const unsigned long long value = std::stoull(text, &consumed);
    if (consumed != text.size() || value == 0 ||
        value > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument(
            std::string("invalid positive integer for ") + option);
    return static_cast<std::uint32_t>(value);
}

float parse_angle(const std::string& text) {
    std::size_t consumed = 0;
    const float angle = std::stof(text, &consumed);
    if (consumed != text.size() || !std::isfinite(angle))
        throw std::invalid_argument("invalid finite number for --angle");
    // 先将角度缩减到一周内，避免有限的大数在转弧度时溢出。
    return std::fmod(angle, 360.0f);
}
}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path output = "minigpu.ppm";
    std::uint32_t width = 256, height = 256;
    std::string backend = "starter";
    std::string scene = "cube";
    float angle = 30.0f;
    bool reverse_order = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto value = [&](const char* option) -> std::string {
                if (i + 1 >= argc)
                    throw std::invalid_argument(
                        std::string("missing value for ") + option);
                return argv[++i];
            };
            if (arg == "--help") {
                usage(argv[0]);
                return 0;
            } else if (arg == "--output")
                output = value("--output");
            else if (arg == "--width")
                width = parse_dimension(value("--width"), "--width");
            else if (arg == "--height")
                height = parse_dimension(value("--height"), "--height");
            else if (arg == "--backend")
                backend = value("--backend");
            else if (arg == "--scene")
                scene = value("--scene");
            else if (arg == "--angle")
                angle = parse_angle(value("--angle"));
            else if (arg == "--reverse-order")
                reverse_order = true;
            else {
                usage(argv[0]);
                return 2;
            }
        }
        if (scene != "cube" && scene != "triangles")
            throw std::invalid_argument("unknown scene: " + scene);
        minigpu::Framebuffer framebuffer(width, height, {24, 32, 48, 255});
        if (backend == "starter") {
            minigpu::CpuRasterizer rasterizer(framebuffer);
            rasterizer.draw_clear({24, 32, 48, 255});
        } else if (backend == "cpu") {
            minigpu::CpuRasterizer rasterizer(framebuffer);
            if (scene == "cube") {
                const auto model = minigpu::scaling({1.5f, 1.1f, 0.7f}) *
                                   minigpu::rotation_y(minigpu::degrees(angle));
                const auto view =
                    minigpu::look_at({3.0f, 2.0f, -6.0f}, {0.0f, 0.0f, 0.0f},
                                     {0.0f, 1.0f, 0.0f});
                const auto projection = minigpu::perspective(
                    minigpu::degrees(60.0f),
                    static_cast<float>(width) / static_cast<float>(height),
                    0.1f, 100.0f);
                auto triangles = minigpu::make_cube_triangles_hardcoded(
                    projection * view * model, width, height);
                if (reverse_order)
                    std::reverse(triangles.begin(), triangles.end());
                for (const auto& triangle : triangles)
                    rasterizer.draw_triangle(triangle);
            } else {
                const float screen_width = static_cast<float>(width);
                const float screen_height = static_cast<float>(height);
                const minigpu::ScreenVertex top_left{{0, 0, 0.5f},
                                                     {255, 0, 0, 255}};
                const minigpu::ScreenVertex top_right{{screen_width, 0, 0.5f},
                                                      {0, 255, 0, 255}};
                const minigpu::ScreenVertex bottom_left{
                    {0, screen_height, 0.5f}, {0, 0, 255, 255}};
                const minigpu::ScreenVertex bottom_right{
                    {screen_width, screen_height, 0.5f}, {255, 255, 255, 255}};
                const minigpu::Triangle triangle1{top_left, top_right,
                                                  bottom_left};
                const minigpu::Triangle triangle2{top_right, bottom_right,
                                                  bottom_left};
                rasterizer.draw_triangle(triangle1);
                rasterizer.draw_triangle_scanLine(triangle2);
            }
        } else if (backend == "cuda-naive") {
            minigpu::run_cuda_demo(framebuffer);
        } else if (backend == "cuda-tiled") {
            throw std::logic_error(
                "tiled CUDA backend is a lab TODO (Weeks 9-10)");
        } else {
            throw std::invalid_argument("unknown backend: " + backend);
        }
        framebuffer.write_ppm(output);
        std::cout << "Wrote " << output << " (" << width << "x" << height
                  << ", " << backend << ")\n";
    } catch (const std::exception& error) {
        std::cerr << "minigpu: " << error.what() << '\n';
        return 1;
    }
}
