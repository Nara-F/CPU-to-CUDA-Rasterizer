#include "render_entry.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "loader.hpp"
#include "scene.hpp"

namespace minigpu {
namespace {

RenderOptions validate(RenderOptions options) {
    if (options.scene != "cube" && options.scene != "triangles" &&
        options.scene != "obj")
        throw std::invalid_argument("unknown scene: " + options.scene);
    if (options.scene == "obj" && options.obj_path.empty())
        throw std::invalid_argument("--scene obj requires --obj PATH");
    if (!options.obj_path.empty() && options.scene != "obj")
        throw std::invalid_argument("--obj requires the obj scene");
    if (options.scene == "obj" && options.backend != "cpu")
        throw std::invalid_argument("OBJ rendering currently requires --backend cpu");
    if (options.custom_transform &&
        (options.backend != "cpu" || options.scene == "triangles"))
        throw std::invalid_argument(
            "camera/model options require --backend cpu and --scene cube or obj");
    return options;
}

}  // namespace

RenderEntry::RenderEntry(RenderOptions options)
    : options_(validate(std::move(options))),
      framebuffer_(options_.width, options_.height, kClearColor) {}

Mat4 RenderEntry::scene_mvp() const {
    const auto& transform = options_.transform;
    // 保持默认画面的 S * Ry 顺序，再执行世界空间平移。
    const auto model = translation(transform.translate) *
                       (scaling(transform.scale) *
                        rotation_y(degrees(transform.angle)));
    const auto view = look_at(transform.eye, transform.target, transform.up);
    const auto projection = perspective(
        degrees(transform.fov),
        static_cast<float>(options_.width) / static_cast<float>(options_.height),
        transform.near_plane, transform.far_plane);
    const auto mvp = projection * view * model;
    for (const auto& column : mvp.m)
        for (float value : column)
            if (!std::isfinite(value))
                throw std::invalid_argument(
                    "camera/model parameters produce a non-finite MVP");
    return mvp;
}

RenderInput RenderEntry::prepare_scene() const {
    RenderInput input;
    if (options_.backend == "starter") return input;

    if (options_.scene == "cube") {
        const auto triangles = make_cube_triangles_hardcoded(
            scene_mvp(), options_.width, options_.height);
        input.triangles.vertices.reserve(triangles.size() * 3);
        for (const auto& triangle : triangles) {
            input.triangles.vertices.push_back(triangle.a);
            input.triangles.vertices.push_back(triangle.b);
            input.triangles.vertices.push_back(triangle.c);
        }
    } else if (options_.scene == "obj") {
        Loader loader;
        auto file = loader.open_file(options_.obj_path);
        const auto mesh = loader.parse_obj(file);
        input.triangles = build_screen_triangles(
            scene_mvp(), mesh, options_.width, options_.height);
    } else {
        const float width = static_cast<float>(options_.width);
        const float height = static_cast<float>(options_.height);
        const ScreenVertex top_left{{0, 0, 0.5f}, {255, 0, 0, 255}};
        const ScreenVertex top_right{{width, 0, 0.5f}, {0, 255, 0, 255}};
        const ScreenVertex bottom_left{{0, height, 0.5f}, {0, 0, 255, 255}};
        const ScreenVertex bottom_right{{width, height, 0.5f}, {255, 255, 255, 255}};
        input.triangles.vertices = {top_left, top_right, bottom_left,
                                    top_right, bottom_right, bottom_left};
        input.scanline_second = true;
    }

    if (options_.reverse_order && options_.scene != "triangles") {
        const auto count = input.triangles.triangle_count();
        for (std::size_t i = 0; i < count / 2; ++i) {
            auto* first = input.triangles.triangle(i);
            auto* last = input.triangles.triangle(count - 1 - i);
            for (std::size_t vertex = 0; vertex < 3; ++vertex)
                std::swap(first[vertex], last[vertex]);
        }
    }
    return input;
}

void RenderEntry::render_backend(const Backend& backend, const RenderInput& input) {
    if (!backend) throw std::invalid_argument("render backend is empty");
    framebuffer_.clear(kClearColor);
    backend(framebuffer_, input);
}

void RenderEntry::run_backend(const Backend& backend) {
    const auto input = prepare_scene();
    render_backend(backend, input);
    framebuffer_.write_ppm(options_.output);
    std::cout << "Wrote " << options_.output << " (" << options_.width << "x"
              << options_.height << ", " << options_.backend << ")\n";
}

}  // namespace minigpu
