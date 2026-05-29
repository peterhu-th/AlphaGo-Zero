cmake_minimum_required(VERSION 3.14)
project(AlphaGoZero CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 寻找 pybind11
find_package(pybind11 REQUIRED)

# 搜集所有源文件
file(GLOB_RECURSE SRC_FILES "cpp_core/*.cpp")

# 使用 pybind11 添加模块
pybind11_add_module(core_engine ${SRC_FILES})

# 指定包含目录
target_include_directories(core_engine PRIVATE cpp_core)

# （可选）为高性能启用 O3 优化
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(core_engine PRIVATE -O3 -Wall -Wextra)
endif()
