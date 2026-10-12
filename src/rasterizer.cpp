#include "minigpu.hpp"

namespace minigpu {

void CpuRasterizer::draw_triangle(const Triangle&) {
    throw std::logic_error(
        "CPU triangle rasterization is a lab TODO (barycentrics, depth, "
        "interpolation)");
}

#if !MINIGPU_HAS_CUDA
void run_cuda_demo(Framebuffer&) {
    throw std::logic_error(
        "CUDA backend is unavailable; configure with -DMINIGPU_ENABLE_CUDA=ON "
        "after installing CUDA");
}
#endif

}  // namespace minigpu
