#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "minigpu.hpp"

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "smoke: " << message << '\n';
        std::exit(1);
    }
}
}  // namespace

int main() {
    minigpu::Framebuffer fb(3, 2, {1, 2, 3, 255});
    check(fb.at(1, 1).g == 2, "initial pixel");
    fb.clear({10, 20, 30, 255});
    check(fb.at(0, 0).r == 10, "clear pixel");
    bool bounds_thrown = false;
    try {
        (void)fb.at(3, 0);
    } catch (const std::out_of_range&) {
        bounds_thrown = true;
    }
    check(bounds_thrown, "bounds check");
    bool dimensions_thrown = false;
    try {
        minigpu::Framebuffer invalid(0, 1);
    } catch (const std::invalid_argument&) {
        dimensions_thrown = true;
    }
    check(dimensions_thrown, "zero dimension check");
    const auto path = std::filesystem::current_path() / "minigpu_smoke.ppm";
    fb.write_ppm(path);
    std::ifstream in(path, std::ios::binary);
    check(static_cast<bool>(in), "PPM open");
    std::string header;
    std::getline(in, header);
    check(header == "P6", "PPM magic");
    std::getline(in, header);
    check(header == "3 2", "PPM dimensions");
    std::getline(in, header);
    check(header == "255", "PPM max value");
    std::string payload((std::istreambuf_iterator<char>(in)), {});
    check(payload.size() == 18, "PPM payload size");
    for (std::size_t i = 0; i < payload.size(); i += 3) {
        check(static_cast<unsigned char>(payload[i]) == 10 &&
                  static_cast<unsigned char>(payload[i + 1]) == 20 &&
                  static_cast<unsigned char>(payload[i + 2]) == 30,
              "PPM RGB payload");
    }
    in.close();
    std::filesystem::remove(path);
}
