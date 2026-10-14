#include <cmath>
#include <stdexcept>

#include "math.hpp"

namespace minigpu {

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

float length(const Vec3& v) { return std::sqrt(dot(v, v)); }

Vec3 normalize(const Vec3& v) {
    const float lenth = length(v);
    if (lenth == 0.0f) { throw std::invalid_argument("a zero-length vector cannot be normalized"); }
    return {v.x / lenth, v.y / lenth, v.z / lenth};
}

Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator-(const Vec3& v) { return {-v.x, -v.y, -v.z}; }

Vec3 operator*(const Vec3& v, float scalar) {
    return {v.x * scalar, v.y * scalar, v.z * scalar};
}

Vec3 operator*(float scalar, const Vec3& v) { return v * scalar; }

Mat4::Mat4() {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row)
            m[column][row] = column == row ? 1.0f : 0.0f;
    }
}

Mat4 translation(const Vec3& v) {
    Mat4 result;
    result.m[3][0] = v.x;
    result.m[3][1] = v.y;
    result.m[3][2] = v.z;
    return result;
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[k][row] * b.m[column][k];
            result.m[column][row] = sum;
        }
    }
    return result;
}

Vec4 operator*(const Mat4& a, const Vec4& v) {
    const float column[4] = {v.x, v.y, v.z, v.w};
    float out[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int row = 0; row < 4; ++row) {
        for (int c = 0; c < 4; ++c) out[row] += a.m[c][row] * column[c];
    }
    return {out[0], out[1], out[2], out[3]};
}

}  // namespace minigpu
