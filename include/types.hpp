#pragma once

#include <cstdint>

#include "math.hpp"

namespace minigpu {

struct Color {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
};

// Screen-space contract: floating-point x/y, origin at the top left,
// +X right and +Y down; pixel (x,y) is sampled at (x+0.5, y+0.5).
// Viewport: x = (ndc_x + 1) * width / 2,
//           y = (1 - ndc_y) * height / 2.
// z = (ndc_z + 1) / 2 is depth in [0,1]: near = 0, far = 1.
// Smaller depth wins; equal depth keeps the first input triangle.
// Color channels are RGBA bytes in [0,255].
struct ScreenVertex {
    Vec3 position;
    Color color{};
};
struct Triangle {
    ScreenVertex a, b, c;
};

}  // namespace minigpu
