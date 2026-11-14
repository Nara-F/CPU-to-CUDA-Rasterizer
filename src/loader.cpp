#include "loader.hpp"

#include <string>

namespace minigpu {
std::ifstream Loader::open_file(const std::filesystem::path& path) {
    if (path.extension() != ".obj") {
        throw std::invalid_argument("File must have .obj extension: " +
                                    path.string());
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    return file;
}

Mesh Loader::parse_obj(std::ifstream& file) {
    std::string line;
    int vertex_count = 0;
    Mesh mesh;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        if (prefix == "#") {
            continue;
        } else if (prefix == "v") {
            float x, y, z;
            if (!(iss >> x >> y >> z)) {
                throw std::invalid_argument(
                    "Invalid vertex line in OBJ file: " + line);
            }
            if (!iss.eof()) {
                char ext;
                while (iss >> ext) {
                    if (ext != '#' && ext != ' ' && ext != '\n') {
                        throw std::invalid_argument(
                            "Invalid vertex line in OBJ file: " + line);
                    } else if (ext == '#') {
                        break;  // Ignore comments after vertex coordinates
                    }
                }
            }
            mesh.positions.push_back({x, y, z});
            vertex_count++;
        } else if (prefix == "f") {
            int64_t a, b, c;
            if (!(iss >> a >> b >> c)) {
                throw std::invalid_argument("Invalid face line in OBJ file: " +
                                            line);
            }
            auto validate = [=](int64_t index) -> uint32_t {
                if (index <= 0 || index > vertex_count) {
                    throw std::invalid_argument(
                        "Face indices out of range in OBJ file");
                }
                return static_cast<uint32_t>(index);
            };
            if (!iss.eof()) {
                char ext;
                while (iss >> ext) {
                    if (ext != '#' && ext != ' ' && ext != '\n') {
                        throw std::invalid_argument(
                            "Invalid face line in OBJ file: " + line);
                    } else if (ext == '#') {
                        break;  // Ignore comments after face indices
                    }
                }
            }
            uint32_t x, y, z;
            x = validate(a);
            y = validate(b);
            z = validate(c);
            if (x == 0 || y == 0 || z == 0 || x > vertex_count ||
                y > vertex_count || z > vertex_count) {
                throw std::invalid_argument(
                    "Face indices out of range in OBJ file");
            }
            mesh.indices.push_back(
                {{x - 1, y - 1, z - 1}});  // 将obj转为0-based索引
        }
    }
    return mesh;
}
}  // namespace minigpu
