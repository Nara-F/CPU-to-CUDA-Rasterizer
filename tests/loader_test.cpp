#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "loader.hpp"

namespace {
class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        std::random_device generator;
        do {
            path_ = std::filesystem::temp_directory_path() /
                    ("minigpu_loader_test_" + std::to_string(generator()));
        } while (!std::filesystem::create_directory(path_));
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::filesystem::path& path() const noexcept { return path_; }
   private:
    std::filesystem::path path_;
};

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void write_obj(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary);
    check(file.is_open(), "cannot create test fixture: " + path.string());
    file << contents;
    file.close();
    check(!file.fail(), "cannot write test fixture: " + path.string());
}

void check_positions(const std::vector<minigpu::Vec3>& actual,
                     const std::vector<minigpu::Vec3>& expected) {
    check(actual.size() == expected.size(), "vertex count mismatch");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto& a = actual[i];
        const auto& e = expected[i];
        check(std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) &&
                  std::fabs(a.x - e.x) <= 1e-6f &&
                  std::fabs(a.y - e.y) <= 1e-6f &&
                  std::fabs(a.z - e.z) <= 1e-6f,
              "position mismatch at vertex " + std::to_string(i));
    }
}
}  // namespace

void invalid_extension_name_test(const std::filesystem::path& directory) {
    minigpu::Loader loader;
    const auto path = directory / "test.txt";
    write_obj(path, "");
    try {
        loader.open_file(path);
    } catch (const std::invalid_argument& e) {
        check(std::string(e.what()) ==
                  "File must have .obj extension: " + path.string(),
              "unexpected invalid-extension diagnostic: " + std::string(e.what()));
        return;
    }
    throw std::runtime_error(
        "Expected invalid_argument exception for invalid extension");
}

void non_existent_file_test(const std::filesystem::path& directory) {
    minigpu::Loader loader;
    const auto path = directory / "nonexistent.obj";
    try {
        loader.open_file(path);
    } catch (const std::runtime_error& e) {
        check(std::string(e.what()) == "Failed to open file: " + path.string(),
              "unexpected missing-file diagnostic: " + std::string(e.what()));
        return;
    }
    throw std::runtime_error(
        "Expected runtime_error exception for non-existent file");
}

void parse_invalid_obj_test(const std::filesystem::path& directory) {
    minigpu::Loader loader;
    const std::string vertices = "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
    struct InvalidCase {
        const char* name;
        std::string contents;
    };
    const std::vector<InvalidCase> cases = {
        {"missing_vertex_coordinate", "v 0 1\n"},
        {"nonnumeric_vertex", "v 0 nope 0\n"},
        {"overflowing_vertex", "v 1e50 0 0\n"},
        {"nan_vertex", "v nan 0 0\n"},
        {"infinite_vertex", "v inf 0 0\n"},
        {"extra_vertex_coordinate", "v 0 0 0 1\n"},
        {"vertex_trailing_junk", "v 0 0 0 junk\n"},
        {"empty_face", vertices + "f\n"},
        {"one_index", vertices + "f 1\n"},
        {"two_indices", vertices + "f 1 2\n"},
        {"quad", vertices + "f 1 2 3 1\n"},
        {"negative_index", vertices + "f -1 2 3\n"},
        {"negative_index_wraparound_first", vertices + "f -4294967295 2 3\n"},
        {"negative_index_wraparound_second", vertices + "f 1 -4294967294 3\n"},
        {"negative_index_wraparound_third", vertices + "f 1 2 -4294967293\n"},
        {"zero_index", vertices + "f 1 0 3\n"},
        {"first_index_out_of_range", vertices + "f 4 2 3\n"},
        {"second_index_out_of_range", vertices + "f 1 4 3\n"},
        {"third_index_out_of_range", vertices + "f 1 2 4\n"},
        {"overflowing_index", vertices + "f 4294967296 2 3\n"},
        {"nonnumeric_index", vertices + "f 1 nope 3\n"},
        {"fractional_index", vertices + "f 1 2 2.5\n"},
        {"face_trailing_junk", vertices + "f 1 2 3 junk\n"},
        {"face_without_vertices", "f 1 2 3\n"},
    };

    std::size_t failures = 0;
    for (const auto& test : cases) {
        const auto path = directory / (std::string(test.name) + ".obj");
        write_obj(path, test.contents);
        auto file = loader.open_file(path);
        try {
            loader.parse_obj(file);
            ++failures;
            std::cerr << test.name << ": expected std::invalid_argument\n";
        } catch (const std::invalid_argument&) {
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << test.name << ": wrong exception: " << error.what() << '\n';
        }
    }
    check(failures == 0, std::to_string(failures) + " invalid OBJ cases failed");
}

void parse_obj_correctness_test(const std::filesystem::path& directory) {
    minigpu::Loader loader;
    const std::string vertices = "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
    const std::vector<minigpu::Vec3> triangle = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    struct ValidCase {
        const char* name;
        std::string contents;
        std::vector<minigpu::Vec3> positions;
        std::vector<minigpu::TriangleIndices> indices;
    };
    const std::vector<ValidCase> cases = {
        {"triangle", vertices + "f 1 2 3\n", triangle, {{0, 1, 2}}},
        {"no_final_newline", vertices + "f 1 2 3", triangle, {{0, 1, 2}}},
        {"shared_vertices_and_winding",
         vertices + "v 1 1 0\nf 3 2 1\nf 2 4 3\n",
         {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},
         {{2, 1, 0}, {1, 3, 2}}},
        {"signed_and_scientific_coordinates",
         "v -1.25 +2.5 3e-1\nv 1e2 -2e-2 0\nv 0 0 -4\nf 1 2 3\n",
         {{-1.25f, 2.5f, 0.3f}, {100.0f, -0.02f, 0.0f}, {0.0f, 0.0f, -4.0f}},
         {{0, 1, 2}}},
        {"tabs_and_blank_lines",
         "\n  v 0 0 0\n\tv\t1\t0\t0\nv 0 1 0\n \n  f 1 2 3\n",
         triangle, {{0, 1, 2}}},
        {"trailing_vertex_whitespace",
         "v 0 0 0  \nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
         triangle, {{0, 1, 2}}},
        {"trailing_face_whitespace", vertices + "f 1 2 3 \t\n",
         triangle, {{0, 1, 2}}},
        {"crlf", "v 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\nf 1 2 3\r\n",
         triangle, {{0, 1, 2}}},
        {"comments_and_ignored_records",
         "#mesh\n\n v 0 0 0 # origin\nv 1 0 0 # right\nv 0 1 0 # top\n"
         "vt 0 0\nvn 0 0 1\no test\ng front\nusemtl plain\n"
         "f 1 2 3 # triangle\n", triangle, {{0, 1, 2}}},
        {"vertices_only", vertices, triangle, {}},
        {"empty", "", {}, {}},
        {"comments_only", "# mesh\n\t#comment\n \n", {}, {}},
    };

    std::size_t failures = 0;
    for (const auto& test : cases) {
        const auto path = directory / (std::string(test.name) + ".obj");
        write_obj(path, test.contents);
        auto file = loader.open_file(path);
        check(file.is_open() && file.good(), "open_file must return a readable stream");
        try {
            const auto mesh = loader.parse_obj(file);
            check_positions(mesh.positions, test.positions);
            check(mesh.indices == test.indices,
                  "zero-based indices, triangle order or winding mismatch");
            check(mesh.colors.empty(), "OBJ without colors must leave colors empty");
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << test.name << ": " << error.what() << '\n';
        }
    }
    check(failures == 0, std::to_string(failures) + " valid OBJ cases failed");
}

int main() {
    try {
        TemporaryDirectory temp_dir;
        invalid_extension_name_test(temp_dir.path());
        non_existent_file_test(temp_dir.path());
        // 分别运行两个解析测试，避免第一组失败掩盖另一组的诊断。
        std::size_t failures = 0;
        for (auto test : {parse_invalid_obj_test, parse_obj_correctness_test}) {
            try {
                test(temp_dir.path());
            } catch (const std::exception& error) {
                ++failures;
                std::cerr << error.what() << '\n';
            }
        }
        check(failures == 0, "OBJ parsing tests failed");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
