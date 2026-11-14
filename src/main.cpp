#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "cuda_backend.hpp"
#include "rasterizer.hpp"
#include "render_entry.hpp"

namespace {
void usage(const char* name) {
    std::cerr << "Usage: " << name
              << " [--help] [--output PATH] [--width N] [--height N] "
                 "[--backend starter|cpu|cuda-naive|cuda-tiled] "
                 "[--scene cube|triangles|obj] [--obj PATH] "
                 "[--angle DEGREES] [--reverse-order] "
                 "[--eye X Y Z] [--target X Y Z] [--up X Y Z] "
                 "[--fov DEGREES] [--near N] [--far N] "
                 "[--translate X Y Z] [--scale X Y Z]\n"
              << "CPU defaults to cube at 30 degrees; --angle and "
                 "--reverse-order apply to cube and obj.\n"
              << "--obj selects the obj scene unless --scene is specified; "
                 "OBJ rendering requires --backend cpu.\n"
              << "Camera/model options apply to CPU cube and obj scenes.\n"
              << "Defaults: eye=(3,2,-6), target=(0,0,0), up=(0,1,0), "
                 "fov=60 degrees, near=0.1, far=100, "
                 "translate=(0,0,0), scale=(1.5,1.1,0.7).\n"
              << "Model order: M = T * (S * Ry); rotate, scale, translate.\n"
              << "Note: --obj is not supported by the CUDA backends.\n";
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

float parse_finite(const std::string& text, const char* option) {
    std::size_t consumed = 0;
    float value;
    try {
        value = std::stof(text, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string("invalid finite number for ") + option);
    }
    if (consumed != text.size() || !std::isfinite(value))
        throw std::invalid_argument(std::string("invalid finite number for ") + option);
    return value;
}

float parse_angle(const std::string& text) {
    const float angle = parse_finite(text, "--angle");
    // 先将角度缩减到一周内，避免有限的大数在转弧度时溢出。
    return std::fmod(angle, 360.0f);
}

}  // namespace

int main(int argc, char** argv) {
    minigpu::RenderOptions options;
    bool scene_specified = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto value = [&](const char* option) -> std::string {
                if (i + 1 >= argc ||
                    std::string(argv[i + 1]).rfind("--", 0) == 0)
                    throw std::invalid_argument(
                        std::string("missing value for ") + option);
                return argv[++i];
            };
            auto vector_value = [&](const char* option) -> minigpu::Vec3 {
                return {parse_finite(value(option), option),
                        parse_finite(value(option), option),
                        parse_finite(value(option), option)};
            };
            if (arg == "--help") {
                usage(argv[0]);
                return 0;
            } else if (arg == "--output")
                options.output = value("--output");
            else if (arg == "--width")
                options.width = parse_dimension(value("--width"), "--width");
            else if (arg == "--height")
                options.height = parse_dimension(value("--height"), "--height");
            else if (arg == "--backend")
                options.backend = value("--backend");
            else if (arg == "--scene") {
                options.scene = value("--scene");
                scene_specified = true;
            } else if (arg == "--obj") {
                options.obj_path = value("--obj");
                if (options.obj_path.empty())
                    throw std::invalid_argument(
                        "--obj requires a non-empty path");
            } else if (arg == "--angle")
                options.transform.angle = parse_angle(value("--angle"));
            else if (arg == "--eye" || arg == "--target" || arg == "--up" ||
                     arg == "--translate" || arg == "--scale") {
                const auto v = vector_value(arg.c_str());
                if (arg == "--eye") options.transform.eye = v;
                else if (arg == "--target") options.transform.target = v;
                else if (arg == "--up") options.transform.up = v;
                else if (arg == "--translate") options.transform.translate = v;
                else options.transform.scale = v;
                options.custom_transform = true;
            } else if (arg == "--fov" || arg == "--near" || arg == "--far") {
                const float v = parse_finite(value(arg.c_str()), arg.c_str());
                if (arg == "--fov") options.transform.fov = v;
                else if (arg == "--near") options.transform.near_plane = v;
                else options.transform.far_plane = v;
                options.custom_transform = true;
            } else if (arg == "--reverse-order")
                options.reverse_order = true;
            else {
                usage(argv[0]);
                return 2;
            }
        }

        if (!options.obj_path.empty() && !scene_specified) options.scene = "obj";
        minigpu::RenderEntry entry(options);
        if (options.backend == "starter" || options.backend == "cpu") {
            minigpu::CpuRasterizer rasterizer(entry.framebuffer());
            entry.run(rasterizer);
        } else if (options.backend == "cuda-naive") {
            entry.run_backend([](minigpu::Framebuffer& target,
                                 const minigpu::RenderInput&) {
                minigpu::run_cuda_demo(target);
            });
        } else if (options.backend == "cuda-tiled") {
            throw std::logic_error(
                "tiled CUDA backend is a lab TODO (Weeks 9-10)");
        } else {
            throw std::invalid_argument("unknown backend: " + options.backend);
        }
    } catch (const std::exception& error) {
        std::cerr << "minigpu: " << error.what() << '\n';
        return 1;
    }
}
