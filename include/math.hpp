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

// M 将模型局部坐标变换到世界坐标。常用组合：
// M = translation(offset) * rotation_y(angle) * scaling(scale)，
// 对列向量依次执行局部缩放、绕原点旋转、世界空间平移。
// 平移矩阵 T：v.x / v.y / v.z 是沿各轴的位移，单位与模型坐标相同。
// 点（w = 1）发生平移，方向（w = 0）不受影响。
Mat4 translation(const Vec3& v);

// 缩放矩阵 S：scale.x / scale.y / scale.z 是各轴的无单位倍率。
// (1,1,1) 为单位缩放；相同倍率为均匀缩放；负值表示轴向镜像，
// 零值将该轴压平。缩放以原点为中心，不改变齐次分量 w。
Mat4 scaling(const Vec3& scale);

// 角度单位：接口只接受 Radians，裸 float 不能隐式转换，避免把"度"误传给
// 要求弧度的参数。调用点用 degrees(60.0f) 或 radians(x) 显式构造。
struct Radians {
    float value;
};

inline constexpr float kPi = 3.14159265358979323846f;
constexpr Radians radians(float value) { return {value}; }
constexpr Radians degrees(float value) { return {value * kPi / 180.0f}; }

// 视图矩阵 V：将世界坐标变换到相机坐标，左手系，相机朝 +Z。
// eye 为相机在世界空间的位置，target 为世界空间中的注视点（不是方向）。
// up 为世界空间的参考上方向（不是位置），不要求单位长度；函数会将其
// 正交化，实际相机上方向垂直于视线。所有输入分量应为有限值。
// eye 映到相机原点，target 映到 +Z 轴；视线为零、up 为零或与视线
// 平行时，normalize 会抛 std::invalid_argument。
Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up);

// 透视投影矩阵 P：将相机坐标变换到齐次裁剪坐标，尚未进行透视除法。
// fov_y 为完整的垂直视场角，单位为弧度；例如 degrees(60.0f)。
// aspect 为画面宽 / 高，调用方须使用浮点除法。
// near_plane / far_plane 为沿相机 +Z 轴到近 / 远裁剪面的正距离，
// 单位与相机坐标相同；它们不是 NDC 深度值，也不是到相机的欧氏距离。
// 调用方须保证所有参数有限且 0 < fov_y.value < kPi。
// 函数检查 aspect > 0 且 0 < near_plane < far_plane，否则抛
// std::invalid_argument。w_clip = z_view，除以 w 后近面 NDC z = -1，
// 远面 NDC z = +1；viewport 再将深度映射到 [0,1]。
Mat4 perspective(Radians fov_y, float aspect, float near_plane,
                 float far_plane);

// 旋转矩阵 R：均以原点为中心，angle 单位为弧度，
// 用 degrees(...) 或 radians(...) 构造，保持齐次分量 w 不变。
// 绕 +X 轴，正 90 度将 +Y 转向 +Z：
//   y' = cos·y - sin·z
//   z' = sin·y + cos·z
Mat4 rotation_x(Radians angle);

// 绕 +Y 轴，正 90 度将 +X 转向 -Z：
//   x' = cos·x + sin·z
//   z' = -sin·x + cos·z
Mat4 rotation_y(Radians angle);

// 绕 +Z 轴，正 90 度将 +X 转向 +Y：
//   x' = cos·x - sin·y
//   y' = sin·x + cos·y
Mat4 rotation_z(Radians angle);

// 矩阵乘法，矩阵和列向量乘法
Mat4 operator*(const Mat4& a, const Mat4& b);
Vec4 operator*(const Mat4& a, const Vec4& v);

}  // namespace minigpu
