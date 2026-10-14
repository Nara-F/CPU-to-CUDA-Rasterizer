# MiniGPU — CPU to CUDA Rasterizer

从 CPU 软件光栅器到 CUDA 分块光栅器的双人项目。通过可复现的正确性与性能实验，解释 GPU 并行和空间分块分别解决了什么问题。

**当前状态：课程脚手架。** 已提供构建入口、PPM 图片输出和小型 OBJ 资产；光栅化、MVP、深度测试、OBJ 解析和 CUDA 算法由 lab 实现。背景图片输出不代表已完成渲染器，未实现的后端必须明确报错。

## 开始

需要 CMake 3.18+ 和支持 C++17 的编译器。CPU 路径不依赖 CUDA、窗口系统或第三方库。

Linux / WSL：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/minigpu --output starter.ppm
```

Windows PowerShell（Visual Studio / MSVC）：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\minigpu.exe --output starter.ppm
```

Windows 与 WSL 可以共用源码，但必须使用不同的构建目录；CMake 缓存记录了系统路径、编译器和生成器，不能跨系统复用。Visual Studio 生成器需要通过 `--config` 和 `-C` 指定配置，`CMAKE_BUILD_TYPE` 不控制它的构建配置。若已构建 Debug，测试时使用 `-C Debug`。

默认 `--backend starter` 仅输出背景；`--backend cpu`、`cuda-naive`、`cuda-tiled` 是后续 lab 的实现入口，当前明确报错。用 `--help` 查看尺寸和输出参数。

CUDA 阶段需要 NVIDIA GPU 和 CUDA Toolkit：

```sh
cmake -S . -B build-cuda -DCMAKE_BUILD_TYPE=Release -DMINIGPU_ENABLE_CUDA=ON
cmake --build build-cuda
```

PPM 是为减少依赖选择的图片格式；最终展示可另行导出 PNG。不要将构建目录、profiling 大文件或临时输出提交进仓库。

```text
include/minigpu.hpp          最小共享类型和接口
src/main.cpp                CLI
src/framebuffer.cpp         背景与 PPM 输出（B）
src/rasterizer.cpp           CPU 光栅化 TODO（B）
src/cuda_backend.cu          可选 CUDA TODO（A）
tests/smoke.cpp              starter 冒烟测试
assets/cube.obj              三角化 cube
labs/README.md              每周任务和验收
docs/performance-report.md  最终报告模板
```

A 的数学模块、B 的 OBJ 与 benchmark 模块在对应 lab 再添加，避免预先堆放空文件。

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
