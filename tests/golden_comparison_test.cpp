#include <iostream>
#include <stdexcept>
#include <string>

#include "golden_comparison.hpp"
#include "rasterizer.hpp"

int main(int argc, char** argv) {
    try {
        minigpu::GoldenOptions options;
        bool cpu_check = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--cpu") cpu_check = true;
            else if (arg == "--output-dir" && i + 1 < argc)
                options.failure_directory = argv[++i];
            else
                throw std::invalid_argument(
                    "Usage: golden_comparison_test [--cpu] [--output-dir PATH]");
        }
        minigpu::RenderEntry::Backend candidate;
        if (cpu_check) {
            std::cout << "Candidate: CPU (comparison/cleanup self-check)\n";
            candidate = [](minigpu::Framebuffer& target, const minigpu::RenderInput& input) {
                minigpu::CpuRasterizer cpu(target);
                minigpu::RenderEntry::with_rasterizer(cpu)(target, input);
            };
        } else {
            std::cout << "Candidate: CUDA placeholder (empty output; kernel not implemented)\n";
            // CUDA 光栅化尚未实现。显式保留清屏结果，实际比较并报告 100 次失败；
            // 后续在此注入消费 RenderInput 的 CUDA 后端，不捕获/忽略真实 CUDA 错误。
            candidate = [](minigpu::Framebuffer&, const minigpu::RenderInput&) {};
        }
        const auto result = minigpu::run_golden_comparison(options, candidate, std::cout);
        return result.failed == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "golden_comparison_test: " << error.what() << '\n';
        return 1;
    }
}
