#pragma once

#include <cstdint>

#include "math.hpp"

namespace minigpu {

struct Color {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
};

// Lab contract: positions are screen coordinates; z is normalized depth in [0,
// 1].
struct ScreenVertex {
    Vec3 position;
    Color color{};
};
struct Triangle {
    ScreenVertex a, b, c;
};

}  // namespace minigpu
