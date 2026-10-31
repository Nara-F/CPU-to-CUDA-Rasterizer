#pragma once

namespace minigpu {

// 坐标契约：左手系；相机空间 +X 向右、+Y 向上、+Z 向前
// 使用列向量，变换顺序 clip = P * V * M * position
// 裁剪空间保持 -w <= x,y,z <= w；透视除法后 NDC 各轴为 [-1,1]，
// 近裁剪面映射到 z = -1，远裁剪面映射到 z = +1。完整契约见 labs/README.md
struct Vec3 {
    float x = 0, y = 0, z = 0;
};
struct Vec4 {
    float x = 0, y = 0, z = 0, w = 1;
};

float dot(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);
float length(const Vec3& v);

// v 为零向量时抛 std::invalid_argument
Vec3 normalize(const Vec3& v);

// 向量运算符重载
Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& v);  // 取负
Vec3 operator*(const Vec3& v, float scalar);
Vec3 operator*(float scalar, const Vec3& v);

// 4x4 矩阵，按列主序存储。m[column][row]
// (M * v)[r]    = 对 c 求和 m[c][r] * v[c]
// (A * B)[c][r] = 对 k 求和 a[k][r] * b[c][k]
struct Mat4 {
    Mat4();
    float m[4][4];
};

// 平移 t；旋转和缩放保持单位矩阵
Mat4 translation(const Vec3& v);

// 角度单位：接口只接受 Radians，裸 float 不能隐式转换，避免把"度"误传给
// 要求弧度的参数。调用点用 degrees(60.0f) 或 radians(x) 显式构造。
struct Radians {
    float value;
};

inline constexpr float kPi = 3.14159265358979323846f;
constexpr Radians radians(float value) { return {value}; }
constexpr Radians degrees(float value) { return {value * kPi / 180.0f}; }

// 视图矩阵：左手系，相机朝 +Z。eye / target / up 为世界空间坐标。
// 视线方向为零向量、或 up 与视线平行时，normalize 会抛 std::invalid_argument。
Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up);

// 透视投影。fov_y 为垂直视场角，near / far 为相机空间 +Z 上的距离。
// 近裁剪面映射到 NDC z = -1，远裁剪面映射到 +1（不是 D3D 的 [0,1]）。
// 要求 aspect > 0 且 0 < near < far，否则抛 std::invalid_argument。
Mat4 perspective(Radians fov_y, float aspect, float near_plane, float far_plane);

// 绕 +Y 轴旋转：
//   x' = cos·x + sin·z
//   z' = -sin·x + cos·z
Mat4 rotation_y(Radians angle);

// 矩阵乘法，矩阵和列向量乘法
Mat4 operator*(const Mat4& a, const Mat4& b);
Vec4 operator*(const Mat4& a, const Vec4& v);

}  // namespace minigpu
