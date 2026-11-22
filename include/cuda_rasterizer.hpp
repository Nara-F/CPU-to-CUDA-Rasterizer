#pragma once

namespace minigpu {

class Framebuffer;
struct RenderInput;

// 在已有 framebuffer 上绘制，返回时颜色和深度已回读到主机。
void render_cuda(Framebuffer& target, const RenderInput& input);

}  // namespace minigpu
