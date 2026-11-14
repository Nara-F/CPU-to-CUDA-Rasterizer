#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "framebuffer.hpp"
#include "triangle_buffer.hpp"

namespace minigpu {

struct SceneOptions {
    Vec3 eye{3.0f, 2.0f, -6.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 translate{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.5f, 1.1f, 0.7f};
    float angle = 30.0f;
    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;
};

struct RenderOptions {
    std::filesystem::path output = "minigpu.ppm";
    std::uint32_t width = 256, height = 256;
    std::string backend = "starter";
    std::string scene = "cube";
    std::filesystem::path obj_path;
    SceneOptions transform;
    bool custom_transform = false;
    bool reverse_order = false;
};

struct RenderInput {
    TriangleBuffer triangles;
    // 保留 Lab02 两种光栅化方法的演示：仅第二个三角形走扫描线。
    bool scanline_second = false;
};

// 参数解析由调用方负责；入口负责配置校验、场景准备、渲染和文件输出。
// 不依赖 CpuRasterizer 或 CUDA 实现，由调用方注入具体实现。
class RenderEntry {
   public:
    using Backend = std::function<void(Framebuffer&, const RenderInput&)>;

    explicit RenderEntry(RenderOptions options);
    Framebuffer& framebuffer() noexcept { return framebuffer_; }

    // Rasterizer 绑定 framebuffer()，须提供 draw_triangle、
    // draw_triangle_scanLine。实现对象在调用期间由调用方持有。
    template <typename Rasterizer>
    void run(Rasterizer& rasterizer) {
        run_backend([&](Framebuffer&, const RenderInput& input) {
            for (std::size_t i = 0; i < input.triangles.triangle_count(); ++i) {
                const auto* v = input.triangles.triangle(i);
                const Triangle triangle{v[0], v[1], v[2]};
                if (input.scanline_second && i == 1)
                    rasterizer.draw_triangle_scanLine(triangle);
                else
                    rasterizer.draw_triangle(triangle);
            }
        });
    }

    // 批量后端接收同样的主机端屏幕三角形输入。
    void run_backend(const Backend& backend);

   private:
    inline static constexpr Color kClearColor{24, 32, 48, 255};
    RenderOptions options_;
    Framebuffer framebuffer_;

    Mat4 scene_mvp() const;
    RenderInput prepare_scene() const;
};

}  // namespace minigpu
