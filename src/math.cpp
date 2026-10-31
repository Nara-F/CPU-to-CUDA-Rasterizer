#include <cmath>
#include <stdexcept>

#include "math.hpp"

namespace minigpu {

namespace {
// Mat4 默认构造是单位矩阵；投影和旋转需要从全零开始。
Mat4 zero_matrix() {
    Mat4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) result.m[column][row] = 0.0f;
    }
    return result;
}
}  // namespace

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

Mat4 scaling(const Vec3& scale) {
    Mat4 result;
    result.m[0][0] = scale.x;
    result.m[1][1] = scale.y;
    result.m[2][2] = scale.z;
    return result;
}

Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
    // 左手系、相机朝 +Z：forward 是 +Z，right = up x forward，
    // camera_up = forward x right。视线为零或 up 与视线平行时 normalize 会抛异常。
    const Vec3 forward = normalize(target - eye);
    const Vec3 right = normalize(cross(up, forward));
    const Vec3 camera_up = cross(forward, right);

    // 视图矩阵的三行分别是 right / up / forward：
    // 行 r 的第 c 个元素存在 m[c][r]（列主序），所以基向量的分量沿
    // m[0][r]、m[1][r]、m[2][r] 存放，不是沿一列存放。
    Mat4 result;
    result.m[0][0] = right.x;
    result.m[1][0] = right.y;
    result.m[2][0] = right.z;

    result.m[0][1] = camera_up.x;
    result.m[1][1] = camera_up.y;
    result.m[2][1] = camera_up.z;

    result.m[0][2] = forward.x;
    result.m[1][2] = forward.y;
    result.m[2][2] = forward.z;

    result.m[3][0] = -dot(right, eye);
    result.m[3][1] = -dot(camera_up, eye);
    result.m[3][2] = -dot(forward, eye);
    return result;
}

Mat4 perspective(Radians fov_y, float aspect, float near_plane,
                 float far_plane) {
    if (!(aspect > 0.0f) || !(near_plane > 0.0f) ||
        !(far_plane > near_plane)) {
        throw std::invalid_argument(
            "perspective requires aspect > 0 and 0 < near < far");
    }

    const float f = 1.0f / std::tan(fov_y.value * 0.5f);
    const float scale_z = (far_plane + near_plane) / (far_plane - near_plane);
    const float offset_z =
        -2.0f * far_plane * near_plane / (far_plane - near_plane);

    Mat4 result = zero_matrix();
    result.m[0][0] = f / aspect;
    result.m[1][1] = f;
    result.m[2][2] = scale_z;
    result.m[3][2] = offset_z;  // 列 3、行 2
    result.m[2][3] = 1.0f;      // 列 2、行 3：w_clip = z_view
    return result;
}

Mat4 rotation_y(Radians angle) {
    const float c = std::cos(angle.value);
    const float s = std::sin(angle.value);

    Mat4 result = zero_matrix();
    result.m[0][0] = c;
    result.m[1][1] = 1.0f;
    result.m[2][2] = c;
    result.m[3][3] = 1.0f;
    result.m[2][0] = s;   // 列 2、行 0
    result.m[0][2] = -s;  // 列 0、行 2
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
