#pragma once

namespace minigpu {

// Coordinate contract: left-handed; camera-space +X points right, +Y up,
// and +Z forward. Use column vectors: clip = P * V * M * position.
// Clip bounds remain -w <= x,y,z <= w; NDC z is -1 at the near plane
// and +1 at the far plane. See labs/README.md for the full contract.
struct Vec3 {
    float x = 0, y = 0, z = 0;
};
struct Vec4 {
    float x = 0, y = 0, z = 0, w = 1;
};

}  // namespace minigpu
