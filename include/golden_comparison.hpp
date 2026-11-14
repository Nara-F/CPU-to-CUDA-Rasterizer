#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>

#include "render_entry.hpp"

namespace minigpu {

struct ComparisonTolerance {
    int rgb = 1;  // 8-bit 通道，1 对应归一化颜色的 1/255。
    float depth = 1e-5f;
};

struct ComparisonResult {
    std::size_t coverage_mismatches = 0;
    std::size_t rgb_mismatches = 0;
    std::size_t depth_mismatches = 0;
    int max_rgb_error = 0;
    float max_depth_error = 0;

    bool passed() const noexcept {
        return coverage_mismatches == 0 && rgb_mismatches == 0 &&
               depth_mismatches == 0;
    }
};

// 覆盖以深度是否有限判定；背景深度必须为 +inf。
ComparisonResult compare_framebuffers(const Framebuffer& reference,
                                      const Framebuffer& candidate,
                                      ComparisonTolerance tolerance = {});

struct GoldenOptions {
    std::uint32_t seed = 1027;  // 与 framebuffer_test 一致。
    std::size_t cases = 100;
    std::filesystem::path failure_directory = "golden_failures";
    ComparisonTolerance tolerance;
};

struct GoldenResult {
    std::size_t passed = 0;
    std::size_t failed = 0;
    // 每次运行使用独立子目录；全部成功时删除该目录并返回空路径。
    std::filesystem::path failure_directory;
};

// 固定种子生成 OBJ，经 RenderEntry 加载及变换，同一屏幕输入分别渲染 CPU
// golden 和注入的候选后端。逐组继续比较，失败保留 OBJ/参数，成功删除 OBJ。
GoldenResult run_golden_comparison(const GoldenOptions& options,
                                   const RenderEntry::Backend& candidate,
                                   std::ostream& log);

}  // namespace minigpu
