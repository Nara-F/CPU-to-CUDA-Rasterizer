# MiniGPU — CPU to CUDA Rasterizer

从 CPU 软件光栅器到 CUDA 分块光栅器的双人项目。通过可复现的正确性与性能实验，解释 GPU 并行和空间分块分别解决了什么问题。

**当前进度：Lab03 三维与深度。** 已实现 M/V/P、裁剪拒绝、透视除法、viewport、CPU Z-buffer 和硬编码索引 cube；保留 Lab02 的重心坐标 / 扫描线光栅化、top-left 覆盖与顶点 RGB 插值。OBJ 解析及 CUDA 算法将在后续 lab 实现。

## 开始

需要 CMake 3.18+ 和支持 C++17 的编译器。CPU 路径不依赖 CUDA、窗口系统或第三方库。

Linux / WSL：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/minigpu --output starter.ppm
./build/minigpu --backend cpu --scene cube --angle 30 --width 256 --height 256 --output lab03.ppm
./build/minigpu --backend cpu --scene cube --angle 30 --reverse-order --output lab03-reverse.ppm
cmake -E compare_files lab03.ppm lab03-reverse.ppm
./build/minigpu --backend cpu --scene triangles --output lab02.ppm
```

Windows PowerShell（Visual Studio / MSVC）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\minigpu.exe --output starter.ppm
.\build\Release\minigpu.exe --backend cpu --scene cube --angle 30 --output lab03.ppm
.\build\Release\minigpu.exe --backend cpu --scene cube --angle 30 --reverse-order --output lab03-reverse.ppm
cmake -E compare_files lab03.ppm lab03-reverse.ppm
.\build\Release\minigpu.exe --backend cpu --scene triangles --output lab02.ppm
```

Windows 与 WSL 可以共用源码，但必须使用不同的构建目录；CMake 缓存记录了系统路径、编译器和生成器，不能跨系统复用。Visual Studio 生成器需要通过 `--config` 和 `-C` 指定配置，`CMAKE_BUILD_TYPE` 不控制它的构建配置。若已构建 Debug，测试时使用 `-C Debug`。

默认 `--backend starter` 仅输出背景。`--backend cpu` 默认绘制 cube：8 个局部顶点、12 个索引三角形，按面固定配色，不做背面剔除，由 Z-buffer 保证遮挡。`--angle` 接受有限的角度值（单位为度，默认 30），控制绕 Y 轴的模型旋转；分别以 0、30、60 度导出即可比较姿态。`--reverse-order` 反转 cube 三角形提交顺序，可用上面的命令验证图像一致。这两个参数用于 cube 场景。

当前 cube 场景调用 `make_cube_triangles_hardcoded()`，是 Lab03 内置顶点、索引和面颜色的演示入口。Lab04 将实现 OBJ 加载与接收网格数据的通用变换入口，`cube.obj` 通过该通用入口渲染。

`--scene triangles` 绘制 Lab02 的两个彩色三角形，分别使用重心坐标和扫描线实现，拼成铺满画面的矩形；共享顶点使用相同颜色，对角线上的颜色连续。`cuda-naive`、`cuda-tiled` 尚未实现，调用会明确报错。用 `--help` 查看尺寸和输出参数。

Lab02 测试覆盖单三角形、六种顶点排列、退化三角形、像素中心与平边、拼接矩形无裂缝及无重复覆盖、固定像素的 RGB 插值，以及非有限坐标的预期异常。三角形测试会在构建目录的 `testPPM/` 下输出两种实现的插值图片，并自动创建该目录。

绘制接口接收屏幕空间顶点：像素中心为 `(x+0.5, y+0.5)`；有限坐标超出画面时，仅遍历画面内的部分；任一顶点的 x/y/z 为 NaN 或无穷、或 z 不在 `[0,1]` 时抛出 `std::runtime_error`，退化三角形直接跳过。边界转换先以 `double` 取整并限制到 `[0, width]` / `[0, height]`，再转整数，循环采用 `< end`。当前循环索引要求宽高不超过 `INT32_MAX`。RGB 使用屏幕空间重心插值，三个顶点的 alpha 必须相同；透视正确颜色插值尚未实现。

两种 CPU 实现均在屏幕空间线性插值归一化深度。Framebuffer 持有颜色和深度，创建及 `clear()` / `draw_clear()` 时深度设为正无穷；仅当新深度严格更小时更新颜色和深度，等深保留先提交者。`depth_at(x,y)` 可读取深度用于验收；直接 `at()` / `set_pixel()` 写颜色不改变深度。Cube 先计算 `P * V * M * position`，检查有限值、`w > 0` 和齐次裁剪边界，再透视除法；任一顶点越界就整三角形丢弃。极端宽高比可能造成边界缺面，常规演示尺寸为 256×256 或 320×240。

Lab03 深度测试覆盖前后顺序交换、两种实现混合绘制、相交平面的逐像素深度、等深首个输入胜出、清屏重置、非法深度，以及 cube 正面遮挡的固定颜色 / 深度期望。还比较多种旋转角度、横屏 / 竖屏下两种实现及正反提交顺序的颜色、覆盖和深度；RGB 容差为 1，深度绝对容差为 `1e-5`。非法 w、相机后方和非有限变换的 cube 被安全拒绝。

CUDA 阶段需要 NVIDIA GPU 和 CUDA Toolkit：

```sh
cmake -S . -B build-cuda -DCMAKE_BUILD_TYPE=Release -DMINIGPU_ENABLE_CUDA=ON
cmake --build build-cuda
```

PPM 是为减少依赖选择的图片格式；最终展示可另行导出 PNG。不要将构建目录、profiling 大文件或临时输出提交进仓库。

```text
include/minigpu.hpp          最小共享类型和接口
src/main.cpp                CLI
src/framebuffer.cpp         颜色 / 深度缓冲与 PPM 输出（B）
src/math.cpp                 基础向量 / 矩阵运算（A）
src/raster_math.cpp          重心坐标、包围盒与 top-left 覆盖（A）
src/rasterizer.cpp           CPU 包围盒 / 扫描线光栅化、颜色插值与深度测试（B）
src/scene.cpp                硬编码索引 cube 与 MVP 投影（B）
src/cuda_backend.cu          可选 CUDA TODO（A）
tests/smoke.cpp              starter 冒烟测试
tests/raster_math_test.cpp   光栅数学与覆盖规则测试
tests/triangle_test.cpp      两种 CPU 实现的覆盖、颜色与异常测试
tests/depth_test.cpp         遮挡、等深、清屏、深度插值与 cube 验收
assets/cube.obj              三角化 cube
labs/README.md              每周任务和验收
docs/performance-report.md  最终报告模板
```

OBJ 与 benchmark 模块在对应 lab 再添加，避免预先堆放空文件。

## 合作方式

采用 **10 周必做主线 + 第 11–12 周缓冲 / 可选优化**，每人每周 6–8 小时。A 固定负责数学、变换和 GPU 路径；B 固定负责 CPU 路径、数据输入、正确性和性能实验。每周都互审核心代码，但不轮换模块负责人。

每周任务、接口约定和验收见 [labs/README.md](labs/README.md)。按 `lab01` 至 `lab10` 组织小型 PR；PR 说明本周目标、实现思路、验证命令和实际结果。推荐每人每周投入 1 小时学习、4–5 小时实现、1 小时测试和互审、最多 1 小时修复。

Git 流程：开分支 → 开发和测试 → 提 PR → 对方看过后合并。具体命令见 [双人 Git 简单约定](docs/git-workflow.md)。

## 最终项目验收

- [ ] CPU 正确显示插值三角形、遮挡关系、旋转 cube 和至少一个 OBJ 模型。
- [ ] CPU、naive CUDA、tiled CUDA 使用同一场景、相机和覆盖规则，自动对比输出。
- [ ] 三个后端均可通过 CLI 从干净检出构建、渲染；CPU 可单独运行。
- [ ] 提交演示图片及完整复现命令，覆盖颜色插值、深度、模型和 GPU 一致性。
- [ ] 固定环境下测量三个后端，覆盖分辨率、三角形数量和 block size。
- [ ] 至少用一次 Nsight 定位真实瓶颈，并用对照实验解释一个有效优化。
- [ ] 完成 [性能报告](docs/performance-report.md)，公开限制、失败案例和各自贡献。
- [ ] 将本 README 更新为作品入口：最终图片、架构、使用方法、实测表、结论和报告链接；标记一个完成版本。

最终支持静态不透明三角网格、MVP、Z-buffer、顶点颜色插值及简单 OBJ。纹理、材质、光照、交互窗口、通用场景图均不属于必做范围。正确性、完整演示和可信测量优先于继续增加功能。

## 最终结果表

实现完成后用同一场景的实测值替换空项；目前没有性能结论。

| Version | Raster time (ms) | End-to-end time (ms) | 场景 / 环境 |
| --- | ---: | ---: | --- |
| CPU | — | — | 待测 |
| Naive CUDA | — | — | 待测 |
| Tiled CUDA | — | — | 待测 |
