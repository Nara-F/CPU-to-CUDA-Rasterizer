#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "golden_comparison.hpp"
#include "rasterizer.hpp"

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
                   ("minigpu_golden_unit_" + std::to_string(random()));
        } while (!std::filesystem::create_directory(path));
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

std::string read(const std::filesystem::path& path) {
    std::ifstream file(path);
    check(file.is_open(), "missing retained failure artifact");
    return std::string(std::istreambuf_iterator<char>(file), {});
}

std::size_t obj_count(const std::filesystem::path& directory) {
    std::size_t count = 0;
    for (const auto& item : std::filesystem::recursive_directory_iterator(directory))
        if (item.path().extension() == ".obj") ++count;
    return count;
}

void render_cpu(minigpu::Framebuffer& target, const minigpu::RenderInput& input) {
    minigpu::CpuRasterizer cpu(target);
    minigpu::RenderEntry::with_rasterizer(cpu)(target, input);
}

void test_comparator() {
    using namespace minigpu;
    Framebuffer reference(2, 1, {24, 32, 48, 255});
    reference.depth_test_and_set_pixel(0, 0, 0.5f, {100, 110, 120, 255});
    Framebuffer close(2, 1, {24, 32, 48, 255});
    close.depth_test_and_set_pixel(0, 0, 0.500005f, {101, 109, 120, 255});
    check(compare_framebuffers(reference, close).passed(), "values within tolerance must pass");
    check(!compare_framebuffers(reference, close, {0, 0}).passed(), "zero tolerance must be honored");
    Framebuffer bad_depth(2, 1, {24, 32, 48, 255});
    bad_depth.depth_test_and_set_pixel(0, 0, 0.50002f, {100, 110, 120, 255});
    const auto depth = compare_framebuffers(reference, bad_depth);
    check(depth.depth_mismatches == 1 && depth.rgb_mismatches == 0 &&
              depth.coverage_mismatches == 0, "depth-only error must fail");
    Framebuffer bad_color = reference;
    bad_color.at(0, 0).r += 2;
    const auto color = compare_framebuffers(reference, bad_color);
    check(color.rgb_mismatches == 1 && color.max_rgb_error == 2 &&
              color.depth_mismatches == 0, "RGB-only error must fail");
    Framebuffer empty(2, 1, {24, 32, 48, 255});
    empty.at(0, 0) = reference.at(0, 0);
    check(compare_framebuffers(reference, empty).coverage_mismatches == 1,
          "coverage must be checked even when RGB matches");
    check(compare_framebuffers(empty, empty).passed(), "background +inf depths must match");
    bool rejected = false;
    try { compare_framebuffers(reference, Framebuffer(1, 1)); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "different dimensions must be rejected");
    rejected = false;
    try { compare_framebuffers(reference, reference, {-1, 1e-5f}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "invalid tolerance must be rejected");
}

void test_suite(const std::filesystem::path& directory) {
    minigpu::GoldenOptions options;
    options.failure_directory = directory;
    std::ofstream(directory / "keep.txt") << "existing artifact";
    std::ostringstream log;
    const auto success = minigpu::run_golden_comparison(options, render_cpu, log);
    check(success.passed == 100 && success.failed == 0 && success.failure_directory.empty(),
          "100 CPU/CPU cases must pass");
    check(std::filesystem::exists(directory / "keep.txt") && obj_count(directory) == 0,
          "success must clean generated OBJ and preserve existing files");
    check(std::distance(std::filesystem::directory_iterator(directory),
                        std::filesystem::directory_iterator{}) == 1,
          "successful run must remove its temporary subdirectory");

    options.cases = 6;
    int calls = 0;
    const auto mixed = minigpu::run_golden_comparison(options,
        [&](minigpu::Framebuffer& target, const minigpu::RenderInput& input) {
            if (calls++ % 2 == 0) render_cpu(target, input);
        }, log);
    check(calls == 6 && mixed.passed == 3 && mixed.failed == 3,
          "suite must continue after failures and classify each case");
    check(obj_count(mixed.failure_directory) == 3 &&
              !std::filesystem::exists(mixed.failure_directory / "case_000.obj") &&
              std::filesystem::exists(mixed.failure_directory / "case_001.obj"),
          "retain only failed OBJ, delete successful OBJ");

    options.cases = 3;
    const auto empty = [](minigpu::Framebuffer&, const minigpu::RenderInput&) {};
    const auto first = minigpu::run_golden_comparison(options, empty, log);
    const auto second = minigpu::run_golden_comparison(options, empty, log);
    check(first.failed == 3 && second.failed == 3, "empty backend must fail every case");
    for (int i = 0; i < 3; ++i) {
        const std::string name = "case_00" + std::to_string(i) + ".obj";
        check(read(first.failure_directory / name) == read(second.failure_directory / name),
              "fixed seed must reproduce identical OBJ");
    }
    const auto report = read(first.failure_directory / "report.txt");
    check(report.find("--eye") != std::string::npos && report.find("--scale") != std::string::npos &&
              report.find("no covered pixels") == std::string::npos,
          "failures need replay parameters and visible CPU reference geometry");

    const auto throwing = minigpu::run_golden_comparison(options,
        [](minigpu::Framebuffer& target, const minigpu::RenderInput& input) {
            render_cpu(target, input);
            throw std::runtime_error("injected failure");
        }, log);
    check(throwing.failed == 3 && obj_count(throwing.failure_directory) == 3,
          "backend exceptions must fail even when pixels match and preserve all cases");
    check(read(throwing.failure_directory / "report.txt").find("injected failure") != std::string::npos,
          "backend exception reason must be retained");
}

}  // namespace

int main() {
    try {
        test_comparator();
        TemporaryDirectory directory;
        test_suite(directory.path);
    } catch (const std::exception& error) {
        std::cerr << "golden_comparison_unit_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
