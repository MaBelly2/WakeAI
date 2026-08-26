cmake_minimum_required(VERSION 3.16)
project(WakeAI VERSION 1.0.0 LANGUAGES CXX)

# ============ C++ 标准 ============
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============ 依赖查找 ============
# OpenCV：通过系统环境变量 OpenCV_DIR 自动定位（不再硬编码路径）
find_package(OpenCV REQUIRED)

# ============ 头文件搜索目录 ============
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ============ 源文件 ============
# 自动收集 src 目录下所有 .cpp 文件
file(GLOB_RECURSE WAKEAI_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# ============ 生成可执行程序 ============
add_executable(WakeAI ${WAKEAI_SOURCES})

# 链接 OpenCV 库
target_link_libraries(WakeAI ${OpenCV_LIBS})

# ============ 打印信息，方便确认是否找到 OpenCV ============
message(STATUS "OpenCV 版本: ${OpenCV_VERSION}")
message(STATUS "OpenCV 库文件: ${OpenCV_LIBS}")
