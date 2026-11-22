#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <system_error>

#include "rasterizer.hpp"
#include "render_entry.hpp"

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory() {
        std::random_device random;
        do {
            path = std::filesystem::temp_directory_path() /
                   ("minigpu_entry_test_" + std::to_string(random()));
        } while (!std::filesystem::create_directory(path));
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

void test_triangles_and_starter(const std::filesystem::path& directory) {
    minigpu::RenderOptions options;
    options.backend = "cpu";
    options.scene = "triangles";
    options.output = directory / "triangles.ppm";
    minigpu::RenderEntry entry(options);
    entry.run_backend(minigpu::render_cpu);
    check(entry.framebuffer().depth_at(0, 0) == 0.5f &&
              entry.framebuffer().depth_at(options.width - 1, options.height - 1) == 0.5f,
          "CPU backend must draw both triangles");
    check(std::filesystem::exists(options.output), "entry must write output");

    options.backend = "starter";
    options.output = directory / "starter.ppm";
    minigpu::RenderEntry starter(options);
    starter.run_backend(minigpu::render_cpu);
    check(std::isinf(starter.framebuffer().depth_at(0, 0)),
          "starter must only clear the framebuffer");
}

void test_obj_and_batch_input(const std::filesystem::path& directory) {
    minigpu::RenderOptions options;
    options.backend = "cpu";
    options.scene = "obj";
    options.obj_path = directory / "triangle.obj";
    options.output = directory / "obj.ppm";
    options.width = options.height = 100;
    options.transform.eye = {0, 0, -5};
    options.transform.scale = {1, 1, 1};
    options.transform.angle = 0;
    options.transform.fov = 90;
    std::ofstream(options.obj_path) << "v -1 -1 0\nv 1 -1 0\nv 0 1 0\nf 1 2 3\n";
    minigpu::RenderEntry entry(options);
    entry.run_backend(minigpu::render_cpu);
    check(entry.framebuffer().at(50, 50).r == 230,
          "injected CPU rasterizer must render the prepared OBJ");
    bool called = false;
    entry.run_backend([&](minigpu::Framebuffer& target,
                          const minigpu::RenderInput& input) {
        called = true;
        check(input.triangles.triangle_count() == 1,
              "batch backend must receive prepared screen triangles");
        check(target.at(50, 50).r == 24,
              "each run must clear previous rendering before dispatch");
    });
    check(called, "batch backend must be called");
}

void test_failed_backend_does_not_write(const std::filesystem::path& directory) {
    minigpu::RenderOptions options;
    options.output = directory / "failed.ppm";
    minigpu::RenderEntry entry(options);
    bool failed = false;
    try {
        entry.run_backend([](minigpu::Framebuffer&, const minigpu::RenderInput&) {
            throw std::runtime_error("backend failed");
        });
    } catch (const std::runtime_error&) {
        failed = true;
    }
    check(failed && !std::filesystem::exists(options.output),
          "backend failure must propagate without writing output");
}

}  // namespace

int main() {
    try {
        TemporaryDirectory directory;
        test_triangles_and_starter(directory.path);
        test_obj_and_batch_input(directory.path);
        test_failed_backend_does_not_write(directory.path);
    } catch (const std::exception& error) {
        std::cerr << "render_entry_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
