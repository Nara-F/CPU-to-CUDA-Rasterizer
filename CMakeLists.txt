cmake_minimum_required(VERSION 3.18)
project(MiniGPU LANGUAGES CXX)

option(MINIGPU_ENABLE_CUDA "Build the optional CUDA backend" OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # 源文件是 UTF-8（含中文注释）。MSVC 默认按本地代码页解析，中文会被当成
  # 非法字符并引发成片的语法错误，必须显式指定源码与执行字符集。
  # 只作用于 C++，避免把 /utf-8 直接丢给 nvcc。
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
endif()

if(MINIGPU_ENABLE_CUDA)
  include(CheckLanguage)
  check_language(CUDA)
  if(NOT CMAKE_CUDA_COMPILER)
    message(FATAL_ERROR "MINIGPU_ENABLE_CUDA=ON, but nvcc was not found. Install the CUDA toolkit or configure with -DMINIGPU_ENABLE_CUDA=OFF.")
  endif()
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
endif()
add_library(minigpu_core src/framebuffer.cpp src/math.cpp src/raster_math.cpp
                          src/rasterizer.cpp)
target_include_directories(minigpu_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(minigpu_core PUBLIC cxx_std_17)

if(MINIGPU_ENABLE_CUDA)
  target_sources(minigpu_core PRIVATE src/cuda_backend.cu)
  target_compile_definitions(minigpu_core PUBLIC MINIGPU_HAS_CUDA=1)
  set_target_properties(minigpu_core PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
else()
  target_compile_definitions(minigpu_core PUBLIC MINIGPU_HAS_CUDA=0)
endif()

add_executable(minigpu src/main.cpp)
target_link_libraries(minigpu PRIVATE minigpu_core)

include(CTest)
if(BUILD_TESTING)
  add_executable(minigpu_smoke tests/smoke.cpp)
  target_link_libraries(minigpu_smoke PRIVATE minigpu_core)
  add_test(NAME minigpu_smoke COMMAND minigpu_smoke)

  add_executable(minigpu_framebuffer_test tests/framebuffer_test.cpp)
  target_link_libraries(minigpu_framebuffer_test PRIVATE minigpu_core)
  add_test(NAME minigpu_framebuffer_test COMMAND minigpu_framebuffer_test)

  add_executable(minigpu_math_test tests/math_test.cpp)
  target_link_libraries(minigpu_math_test PRIVATE minigpu_core)
  add_test(NAME minigpu_math_test COMMAND minigpu_math_test)

  add_executable(minigpu_raster_math_test tests/raster_math_test.cpp)
  target_link_libraries(minigpu_raster_math_test PRIVATE minigpu_core)
  add_test(NAME minigpu_raster_math_test COMMAND minigpu_raster_math_test)

  add_executable(minigpu_triangle_test tests/triangle_test.cpp)
  target_link_libraries(minigpu_triangle_test PRIVATE minigpu_core)
  add_test(NAME minigpu_triangle_test COMMAND minigpu_triangle_test)
endif()
