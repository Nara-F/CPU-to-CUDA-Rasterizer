#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "types.hpp"

namespace minigpu {

class Loader {
   public:
    Loader() {}
    ~Loader() {}
    std::ifstream open_file(const std::filesystem::path& path);

    // 解析obj文件，返回顶点坐标和三角形索引
    Mesh parse_obj(std::ifstream& file);
};

}  // namespace minigpu
