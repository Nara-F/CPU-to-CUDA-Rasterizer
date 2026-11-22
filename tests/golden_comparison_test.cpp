#include "golden_comparison.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include "cuda_rasterizer.hpp"
#include "rasterizer.hpp"

int main(int argc, char** argv) {
    try {
        minigpu::GoldenOptions options;
        bool cpu_check = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--cpu")
                cpu_check = true;
            else if (arg == "--output-dir" && i + 1 < argc)
                options.failure_directory = argv[++i];
            else
                throw std::invalid_argument(
                    "Usage: golden_comparison_test [--cpu] [--output-dir "
                    "PATH]");
        }
        minigpu::RenderEntry::Backend candidate;
        if (cpu_check) {
            std::cout << "Candidate: CPU (comparison/cleanup self-check)\n";
            candidate = minigpu::render_cpu;
        } else {
#if MINIGPU_HAS_CUDA
            std::cout << "Candidate: CUDA placeholder\n";
            candidate = minigpu::render_cuda;
#else
            std::cout << "Candidate: CUDA (not available, skipping)\n";
            candidate = [](minigpu::Framebuffer&, const minigpu::RenderInput&) {
                throw std::runtime_error("CUDA backend not available");
            };
#endif
        }
        const auto result =
            minigpu::run_golden_comparison(options, candidate, std::cout);
        return result.failed == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "golden_comparison_test: " << error.what() << '\n';
        return 1;
    }
}
