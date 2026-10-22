#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "minigpu.hpp"

namespace {

constexpr minigpu::Color kBackground{12, 23, 34, 255};
constexpr minigpu::Color kFill{200, 150, 100, 255};

// Each implementation runs the same triangle coverage contract. Add another
// adapter here when its renderer is ready; the test cases stay unchanged.
struct TriangleImplementation {
    const std::string& name;
    void (*draw)(minigpu::Framebuffer&, const minigpu::Triangle&);
};

const TriangleImplementation kImplementations[] = {
    {"scanline",
     [](minigpu::Framebuffer& target, const minigpu::Triangle& triangle) {
         minigpu::CpuRasterizer rasterizer(target);
         rasterizer.draw_triangle_scanLine(triangle);
     }},
    {"barycentric",
     [](minigpu::Framebuffer& target, const minigpu::Triangle& triangle) {
         minigpu::CpuRasterizer rasterizer(target);
         rasterizer.draw_triangle(triangle);
     }}};

minigpu::Triangle triangle(const std::array<minigpu::Vec3, 3>& positions) {
    return {
        {positions[0], kFill}, {positions[1], kFill}, {positions[2], kFill}};
}

template <typename Expected>
void check_pixels(const minigpu::Framebuffer& fb, const std::string& name,
                  Expected expected) {
    for (unsigned y = 0; y < fb.height(); ++y) {
        for (unsigned x = 0; x < fb.width(); ++x) {
            const auto want = expected(x, y) ? kFill : kBackground;
            const auto got = fb.at(x, y);
            if (got.r != want.r || got.g != want.g || got.b != want.b ||
                got.a != want.a) {
                throw std::runtime_error(name + " at pixel (" +
                                         std::to_string(x) + "," +
                                         std::to_string(y) + ")");
            }
        }
    }
}

// Check all six vertex orders so flat-edge handling cannot depend on input
// order. expected(x,y) receives integer pixel indices and returns coverage.
template <typename Expected>
void check_orders(const TriangleImplementation& implementation,
                  const std::array<minigpu::Vec3, 3>& positions,
                  const std::string& name, Expected expected) {
    std::array<int, 3> order{0, 1, 2};
    do {
        minigpu::Framebuffer fb(8, 8, kBackground);
        implementation.draw(fb,
                            triangle({positions[order[0]], positions[order[1]],
                                      positions[order[2]]}));
        check_pixels(fb,
                     name + " order " + std::to_string(order[0]) +
                         std::to_string(order[1]) + std::to_string(order[2]),
                     expected);
    } while (std::next_permutation(order.begin(), order.end()));
}

void test_degenerate(const TriangleImplementation& implementation) {
    const std::array<minigpu::Vec3, 3> cases[] = {
        {{{1, 2, 0}, {3, 2, 0.5f}, {5, 2, 1}}},  // horizontal
        {{{2, 1, 0}, {2, 3, 0.5f}, {2, 5, 1}}},  // vertical
        {{{1, 1, 0}, {3, 3, 0.5f}, {5, 5, 1}}},  // diagonal
        {{{1, 5, 0}, {3, 3, 0.5f}, {5, 1, 1}}},  // opposite diagonal
        {{{1, 1, 0}, {1, 1, 1}, {5, 4, 0.5f}}},  // same x/y, different z
        {{{3, 3, 0}, {3, 3, 0.5f}, {3, 3, 1}}},  // all coincident
    };
    for (const auto& positions : cases)
        check_orders(implementation, positions, "degenerate",
                     [](unsigned, unsigned) { return false; });
}

void test_flat_edges_and_pixel_centers(
    const TriangleImplementation& implementation) {
    check_orders(implementation, {{{1, 1, 0.5f}, {5, 1, 0.5f}, {1, 5, 0.5f}}},
                 "flat top", [](unsigned x, unsigned y) {
                     return x >= 1 && y >= 1 && x + y < 5;
                 });
    check_orders(implementation, {{{1, 1, 0.5f}, {5, 5, 0.5f}, {1, 5, 0.5f}}},
                 "flat bottom", [](unsigned x, unsigned y) {
                     return x >= 1 && y < 5 && x < y;
                 });
    // The horizontal top passes through pixel centers and must be included.
    check_orders(implementation,
                 {{{1.5f, 1.5f, 0.5f}, {5.5f, 1.5f, 0.5f}, {1.5f, 5.5f, 0.5f}}},
                 "half-integer vertices", [](unsigned x, unsigned y) {
                     return x >= 1 && y >= 1 && x + y < 6;
                 });
    check_orders(implementation, {{{1, 1, 0.5f}, {5, 3, 0.5f}, {1, 5, 0.5f}}},
                 "non-flat", [](unsigned x, unsigned y) {
                     if (y < 1 || y >= 5 || x < 1) return false;
                     const unsigned right = y < 3 ? 2 * y : 10 - 2 * y;
                     return x < right;
                 });
}

void test_nonfinite_coordinates(const TriangleImplementation& implementation) {
    const float bad[] = {std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()};
    for (float value : bad) {
        check_orders(implementation,
                     {{{value, 1, 0.5f}, {5, 1, 0.5f}, {1, 5, 0.5f}}},
                     "nonfinite x", [](unsigned, unsigned) { return false; });
        check_orders(implementation,
                     {{{1, value, 0.5f}, {5, 1, 0.5f}, {1, 5, 0.5f}}},
                     "nonfinite y", [](unsigned, unsigned) { return false; });
    }
}

void test_rectangle_fill(const TriangleImplementation& implementation) {
    const std::array<minigpu::Vec3, 3> first = {
        {{1, 1, 0.5f}, {5, 1, 0.5f}, {1, 5, 0.5f}}};
    const std::array<minigpu::Vec3, 3> second = {
        {{5, 1, 0.5f}, {5, 5, 0.5f}, {1, 5, 0.5f}}};
    const auto expected_rectangle = [](unsigned x, unsigned y) {
        return x >= 1 && x < 5 && y >= 1 && y < 5;
    };

    // The shared edge is screen x+y=6, or pixel-index x+y=5.
    // Its pixel centers belong to the second triangle under top-left coverage.
    check_orders(implementation, first, "rectangle first",
                 [expected_rectangle](unsigned x, unsigned y) {
                     return expected_rectangle(x, y) && x + y < 5;
                 });
    check_orders(implementation, second, "rectangle second",
                 [expected_rectangle](unsigned x, unsigned y) {
                     return expected_rectangle(x, y) && x + y >= 5;
                 });

    // Separate images expose double ownership hidden by same-color overdraw.
    minigpu::Framebuffer first_fb(8, 8, kBackground);
    minigpu::Framebuffer second_fb(8, 8, kBackground);
    implementation.draw(first_fb, triangle(first));
    implementation.draw(second_fb, triangle(second));
    for (unsigned y = 0; y < 8; ++y) {
        for (unsigned x = 0; x < 8; ++x) {
            const int hits = (first_fb.at(x, y).r == kFill.r ? 1 : 0) +
                             (second_fb.at(x, y).r == kFill.r ? 1 : 0);
            const int want = expected_rectangle(x, y) ? 1 : 0;
            if (hits != want) {
                throw std::runtime_error(
                    "rectangle coverage at pixel (" + std::to_string(x) + "," +
                    std::to_string(y) + "): expected " + std::to_string(want) +
                    " hit(s), got " + std::to_string(hits));
            }
        }
    }

    // Also render together in both orders, checking that the second draw
    // preserves pixels produced by the first draw.
    for (bool reverse : {false, true}) {
        minigpu::Framebuffer combined(8, 8, kBackground);
        implementation.draw(combined, triangle(reverse ? second : first));
        implementation.draw(combined, triangle(reverse ? first : second));
        check_pixels(combined, reverse ? "rectangle reverse" : "rectangle",
                     expected_rectangle);
    }
}

void test_export_ppm(const TriangleImplementation& implementation) {
    minigpu::Framebuffer fb(1200, 1200, kBackground);
    minigpu::CpuRasterizer rasterizer(fb);
    minigpu::Triangle testTri({{{0, 0, 0.5f}, {255, 0, 0, 255}},
                               {{1000, 0, 0.5f}, {0, 255, 0, 255}},
                               {{0, 1000, 0.5f}, {0, 0, 255, 255}}});
    implementation.draw(fb, testTri);
    const auto path = std::filesystem::absolute(
        "testPPM/" + std::string("triangle_") + implementation.name + ".ppm");
    rasterizer.export_ppm(fb, path);
    if (!exists(path)) {
        throw std::runtime_error("PPM file not created: " + path.string());
    }
    std::cout << "Wrote " << path << '\n';
}
}  // namespace

int main() {
    for (const auto& implementation : kImplementations) {
        try {
            test_rectangle_fill(implementation);
            test_degenerate(implementation);
            test_flat_edges_and_pixel_centers(implementation);
            test_nonfinite_coordinates(implementation);
            test_export_ppm(implementation);
        } catch (const std::exception& error) {
            std::cerr << "triangle_test [" << implementation.name
                      << "]: " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
