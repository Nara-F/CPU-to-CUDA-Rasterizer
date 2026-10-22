#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "minigpu.hpp"

namespace {
class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        std::random_device generator;
        do {
            path_ = std::filesystem::temp_directory_path() /
                    ("minigpu_framebuffer_test_" + std::to_string(generator()));
        } while (!std::filesystem::create_directory(path_));
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::filesystem::path& path() const noexcept { return path_; }
    void cleanup() { std::filesystem::remove_all(path_); }

   private:
    std::filesystem::path path_;
};

void check_all_pixels(const minigpu::Framebuffer& fb, minigpu::Color expected,
                      const char* message) {
    for (std::uint32_t y = 0; y < fb.height(); ++y) {
        for (std::uint32_t x = 0; x < fb.width(); ++x) {
            const auto& pixel = fb.at(x, y);
            if (pixel.r != expected.r || pixel.g != expected.g ||
                pixel.b != expected.b || pixel.a != expected.a) {
                std::ostringstream error;
                error << message << " failed at (" << x << ", " << y
                      << "), expected RGBA (" << static_cast<int>(expected.r)
                      << ", " << static_cast<int>(expected.g) << ", "
                      << static_cast<int>(expected.b) << ", "
                      << static_cast<int>(expected.a) << ")";
                throw std::runtime_error(error.str());
            }
        }
    }
}

minigpu::Framebuffer test_framebuffer_constructor(std::uint32_t width,
                                                  std::uint32_t height,
                                                  minigpu::Color clear) {
    minigpu::Framebuffer fb(width, height, clear);

    if (fb.width() != width || fb.height() != height) {
        std::ostringstream error;
        error << "test_framebuffer_constructor failed: expected dimensions ("
              << width << ", " << height << "), got (" << fb.width() << ", "
              << fb.height() << ")";
        throw std::runtime_error(error.str());
    }
    check_all_pixels(fb, clear, "test_framebuffer_constructor");
    return fb;
}

void test_invalid_construction() {
    try {
        minigpu::Framebuffer fb(0, 100, {0, 0, 0, 255});
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(
        "test_invalid_construction failed: expected std::invalid_argument");
}

void test_framebuffer_clear(minigpu::Framebuffer& fb, std::mt19937& gen) {
    std::uniform_int_distribution<int> channel(0, 255);
    const minigpu::Color clear = {static_cast<std::uint8_t>(channel(gen)),
                                  static_cast<std::uint8_t>(channel(gen)),
                                  static_cast<std::uint8_t>(channel(gen)), 255};
    fb.clear(clear);
    check_all_pixels(fb, clear, "test_framebuffer_clear");
}

void test_frambuffer_set_pixel(minigpu::Framebuffer& fb, std::mt19937& gen) {
    std::uniform_int_distribution<std::uint32_t> x_dist(0, fb.width() - 1);
    std::uniform_int_distribution<std::uint32_t> y_dist(0, fb.height() - 1);
    std::uniform_int_distribution<int> channel(0, 255);

    const std::uint32_t x = x_dist(gen);
    const std::uint32_t y = y_dist(gen);
    const minigpu::Color color = {static_cast<std::uint8_t>(channel(gen)),
                                  static_cast<std::uint8_t>(channel(gen)),
                                  static_cast<std::uint8_t>(channel(gen)), 255};
    fb.set_pixel(x, y, color);

    const auto& pixel = fb.at(x, y);
    if (pixel.r != color.r || pixel.g != color.g || pixel.b != color.b ||
        pixel.a != color.a) {
        throw std::runtime_error(
            "test_framebuffer_set_pixel failed: expected RGBA (" +
            std::to_string(color.r) + ", " + std::to_string(color.g) + ", " +
            std::to_string(color.b) + ", " + std::to_string(color.a) +
            "), got (" + std::to_string(pixel.r) + ", " +
            std::to_string(pixel.g) + ", " + std::to_string(pixel.b) + ", " +
            std::to_string(pixel.a) + ")");
    }
}

void test_framebuffer_write_ppm(const minigpu::Framebuffer& fb,
                                const std::filesystem::path& directory) {
    const auto path = directory / "framebuffer_test.ppm";
    fb.write_ppm(path);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open PPM output: " + path.string());
    }

    std::string header;
    std::getline(in, header);
    if (header != "P6") {
        throw std::runtime_error("expected PPM magic P6, got " + header);
    }
    std::getline(in, header);
    if (header !=
        std::to_string(fb.width()) + " " + std::to_string(fb.height())) {
        throw std::runtime_error(
            "expected PPM dimensions " + std::to_string(fb.width()) + " " +
            std::to_string(fb.height()) + ", got " + header);
    }
    std::getline(in, header);
    if (header != "255") {
        throw std::runtime_error("expected PPM max value 255, got " + header);
    }
    std::string payload((std::istreambuf_iterator<char>(in)), {});
    const std::size_t expected_size =
        static_cast<std::size_t>(fb.width()) * fb.height() * 3;
    if (payload.size() != expected_size) {
        throw std::runtime_error("expected PPM payload size " +
                                 std::to_string(expected_size) + ", got " +
                                 std::to_string(payload.size()));
    }
    for (std::uint32_t y = 0; y < fb.height(); ++y) {
        for (std::uint32_t x = 0; x < fb.width(); ++x) {
            const auto& pixel = fb.at(x, y);
            const std::size_t offset =
                (static_cast<std::size_t>(y) * fb.width() + x) * 3;
            if (static_cast<unsigned char>(payload[offset]) != pixel.r ||
                static_cast<unsigned char>(payload[offset + 1]) != pixel.g ||
                static_cast<unsigned char>(payload[offset + 2]) != pixel.b) {
                throw std::runtime_error("PPM RGB mismatch at (" +
                                         std::to_string(x) + ", " +
                                         std::to_string(y) + ")");
            }
        }
    }
}

void test_invalid_path_test_framebuffer_write_ppm(
    const minigpu::Framebuffer& fb, const std::filesystem::path& directory) {
    try {
        const auto path = directory / "nonexistent" / "framebuffer_test.ppm";
        fb.write_ppm(path);
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error(
        "invalid PPM path did not throw std::runtime_error");
}

}  // namespace

int main() {
    try {
        constexpr int seed = 1027;
        std::mt19937 gen(seed);
        std::uniform_int_distribution<int> channel(0, 255);
        std::uint8_t r = static_cast<std::uint8_t>(channel(gen));
        std::uint8_t g = static_cast<std::uint8_t>(channel(gen));
        std::uint8_t b = static_cast<std::uint8_t>(channel(gen));

        std::uint32_t width = static_cast<std::uint32_t>(
            std::uniform_int_distribution<int>(1, 1440)(gen));
        std::uint32_t height = static_cast<std::uint32_t>(
            std::uniform_int_distribution<int>(1, 2560)(gen));

        test_invalid_construction();
        auto fb = test_framebuffer_constructor(width, height, {r, g, b, 255});
        test_framebuffer_clear(fb, gen);
        TemporaryDirectory directory;
        test_framebuffer_write_ppm(fb, directory.path());
        test_frambuffer_set_pixel(fb, gen);

        minigpu::Framebuffer patterned(3, 2);
        for (std::uint32_t y = 0; y < patterned.height(); ++y) {
            for (std::uint32_t x = 0; x < patterned.width(); ++x) {
                patterned.at(x, y) = {
                    static_cast<std::uint8_t>(10 + x * 70 + y * 20),
                    static_cast<std::uint8_t>(255 - x * 40 - y * 15),
                    static_cast<std::uint8_t>(13 + x * 45 + y * 30), 123};
            }
        }
        test_framebuffer_write_ppm(patterned, directory.path());
        test_invalid_path_test_framebuffer_write_ppm(fb, directory.path());
        directory.cleanup();
    } catch (const std::exception& error) {
        std::cerr << "framebuffer_test: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
