#include "golden_comparison.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#include "rasterizer.hpp"

namespace minigpu {
namespace {

void validate_tolerance(ComparisonTolerance tolerance) {
    if (tolerance.rgb < 0 || tolerance.rgb > 255 ||
        !std::isfinite(tolerance.depth) || tolerance.depth < 0)
        throw std::invalid_argument("invalid golden comparison tolerance");
}

int sample(std::mt19937& generator, int low, int high) {
    return std::uniform_int_distribution<int>(low, high)(generator);
}

RenderOptions generate_case(std::mt19937& generator, std::size_t index,
                            std::uint32_t seed,
                            const std::filesystem::path& directory) {
    RenderOptions options;
    options.backend = "cpu";  // 场景准备使用 CPU 配置，候选实现另外注入。
    options.scene = "obj";
    options.width = static_cast<std::uint32_t>(sample(generator, 96, 160));
    options.height = static_cast<std::uint32_t>(sample(generator, 64, 128));
    auto& t = options.transform;
    t.eye = {0, 0, -6};
    t.angle = static_cast<float>(sample(generator, -45, 45));
    t.fov = static_cast<float>(sample(generator, 55, 80));
    t.far_plane = 30;
    t.translate = {sample(generator, -20, 20) / 100.0f,
                   sample(generator, -20, 20) / 100.0f,
                   sample(generator, -20, 20) / 100.0f};
    t.scale = {sample(generator, 70, 120) / 100.0f,
               sample(generator, 70, 120) / 100.0f,
               sample(generator, 70, 120) / 100.0f};
    options.reverse_order = sample(generator, 0, 1) != 0;
    std::ostringstream name;
    name << "case_" << std::setw(3) << std::setfill('0') << index << ".obj";
    options.obj_path = directory / name.str();

    std::ofstream obj(options.obj_path);
    obj.imbue(std::locale::classic());
    obj << std::setprecision(std::numeric_limits<float>::max_digits10);
    obj << "# seed=" << seed << " case=" << index << '\n';
    // 固定的可见、非退化三角形防止空画面互相比对而假通过。
    obj << "v -1 -1 0\nv 1 -1 0\nv 0 1 0\nf 1 2 3\n";
    const int count = sample(generator, 4, 24);
    for (int triangle = 1; triangle < count; ++triangle) {
        for (int vertex = 0; vertex < 3; ++vertex) {
            const float x = sample(generator, -140, 140) / 100.0f;
            const float y = sample(generator, -140, 140) / 100.0f;
            const float z = sample(generator, -100, 100) / 100.0f;
            obj << "v " << x << ' ' << y << ' ' << z << '\n';
        }
        obj << "f " << triangle * 3 + 1 << ' ' << triangle * 3 + 2 << ' '
            << triangle * 3 + 3 << '\n';
    }
    // 等深、同几何、不同配色的提交顺序情形。
    if (index % 10 == 0) obj << "f 1 2 3\n";
    obj.close();
    if (!obj) throw std::runtime_error("cannot write OBJ: " + options.obj_path.string());
    return options;
}

void write_parameters(std::ostream& out, const RenderOptions& o) {
    const auto& t = o.transform;
    out << "--backend cpu --scene obj --obj " << std::quoted(o.obj_path.string())
        << " --width " << o.width << " --height " << o.height
        << " --angle " << t.angle
        << " --eye " << t.eye.x << ' ' << t.eye.y << ' ' << t.eye.z
        << " --target " << t.target.x << ' ' << t.target.y << ' ' << t.target.z
        << " --up " << t.up.x << ' ' << t.up.y << ' ' << t.up.z
        << " --fov " << t.fov << " --near " << t.near_plane << " --far " << t.far_plane
        << " --translate " << t.translate.x << ' ' << t.translate.y << ' ' << t.translate.z
        << " --scale " << t.scale.x << ' ' << t.scale.y << ' ' << t.scale.z;
    if (o.reverse_order) out << " --reverse-order";
    out << '\n';
}

}  // namespace

ComparisonResult compare_framebuffers(const Framebuffer& reference,
                                       const Framebuffer& candidate,
                                       ComparisonTolerance tolerance) {
    validate_tolerance(tolerance);
    if (reference.width() != candidate.width() || reference.height() != candidate.height())
        throw std::invalid_argument("golden framebuffer dimensions differ");
    ComparisonResult result;
    for (std::uint32_t y = 0; y < reference.height(); ++y) {
        for (std::uint32_t x = 0; x < reference.width(); ++x) {
            const auto& a = reference.at(x, y);
            const auto& b = candidate.at(x, y);
            const int rgb_error = std::max({std::abs(int(a.r) - int(b.r)),
                                             std::abs(int(a.g) - int(b.g)),
                                             std::abs(int(a.b) - int(b.b))});
            result.max_rgb_error = std::max(result.max_rgb_error, rgb_error);
            if (rgb_error > tolerance.rgb) ++result.rgb_mismatches;

            const float za = reference.depth_at(x, y);
            const float zb = candidate.depth_at(x, y);
            const bool ca = std::isfinite(za), cb = std::isfinite(zb);
            if (ca != cb) ++result.coverage_mismatches;
            const bool valid_a = (ca && za >= 0 && za <= 1) ||
                                 za == std::numeric_limits<float>::infinity();
            const bool valid_b = (cb && zb >= 0 && zb <= 1) ||
                                 zb == std::numeric_limits<float>::infinity();
            float depth_error = 0;
            if (!valid_a || !valid_b || ca != cb)
                depth_error = std::numeric_limits<float>::infinity();
            else if (ca && cb)
                depth_error = std::fabs(za - zb);
            result.max_depth_error = std::max(result.max_depth_error, depth_error);
            if (depth_error > tolerance.depth) ++result.depth_mismatches;
        }
    }
    return result;
}

GoldenResult run_golden_comparison(const GoldenOptions& options,
                                   const RenderEntry::Backend& candidate,
                                   std::ostream& log) {
    validate_tolerance(options.tolerance);
    if (!candidate || options.cases == 0 || options.failure_directory.empty())
        throw std::invalid_argument("golden requires a backend, cases and artifact directory");
    std::filesystem::create_directories(options.failure_directory);
    std::random_device directory_id;  // 仅目录名随机，不参与场景生成。
    std::filesystem::path directory;
    do {
        directory = options.failure_directory /
                    ("seed_" + std::to_string(options.seed) + "_run_" +
                     std::to_string(directory_id()));
    } while (!std::filesystem::create_directory(directory));
    directory = std::filesystem::absolute(directory);

    std::mt19937 generator(options.seed);
    GoldenResult result;
    log << "Golden seed=" << options.seed << " cases=" << options.cases
        << " rgb_tolerance=" << options.tolerance.rgb
        << " depth_tolerance=" << options.tolerance.depth << '\n';
    for (std::size_t i = 0; i < options.cases; ++i) {
        const auto render_options = generate_case(generator, i, options.seed, directory);
        ComparisonResult comparison;
        std::string error;
        try {
            RenderEntry entry(render_options);
            const auto input = entry.prepare_scene();
            CpuRasterizer cpu(entry.framebuffer());
            entry.render_backend(RenderEntry::with_rasterizer(cpu), input);
            const Framebuffer reference = entry.framebuffer();
            bool covered = false;
            for (std::uint32_t y = 0; y < reference.height(); ++y)
                for (std::uint32_t x = 0; x < reference.width(); ++x)
                    covered = covered || std::isfinite(reference.depth_at(x, y));
            if (!covered) throw std::runtime_error("CPU golden has no covered pixels");
            try {
                entry.render_backend(candidate, input);
            } catch (const std::exception& backend_error) {
                error = std::string("candidate backend: ") + backend_error.what();
            }
            comparison = compare_framebuffers(reference, entry.framebuffer(), options.tolerance);
        } catch (const std::exception& case_error) {
            error = std::string("scene/reference/comparison: ") + case_error.what();
        }
        if (error.empty() && comparison.passed()) {
            ++result.passed;
            std::filesystem::remove(render_options.obj_path);
            continue;
        }
        ++result.failed;
        std::ostringstream details;
        details.imbue(std::locale::classic());
        details << std::setprecision(std::numeric_limits<float>::max_digits10)
                << "FAIL case=" << i << " seed=" << options.seed
                << " coverage=" << comparison.coverage_mismatches
                << " rgb=" << comparison.rgb_mismatches
                << " depth=" << comparison.depth_mismatches
                << " max_rgb=" << comparison.max_rgb_error
                << " max_depth=" << comparison.max_depth_error;
        if (!error.empty()) details << " error=" << error;
        details << '\n';
        write_parameters(details, render_options);
        std::ofstream report(directory / "report.txt", std::ios::app);
        if (result.failed == 1)
            report << "seed=" << options.seed << " cases=" << options.cases
                   << " rgb_tolerance=" << options.tolerance.rgb
                   << " depth_tolerance=" << options.tolerance.depth << '\n';
        report << details.str();
        report.close();
        if (!report) throw std::runtime_error("cannot write golden failure report");
        log << details.str();
    }
    if (result.failed == 0) std::filesystem::remove(directory);
    else result.failure_directory = directory;
    log << "Golden: " << result.passed << '/' << options.cases << " passed, "
        << result.failed << " failed";
    if (result.failed) log << "; OBJ/report: " << directory;
    log << '\n';
    return result;
}

}  // namespace minigpu
