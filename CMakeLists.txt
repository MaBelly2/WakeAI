cmake_minimum_required(VERSION 3.16)
project(WakeAI VERSION 1.0.0 LANGUAGES CXX)

# ============ C++ 标准 ============
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============ 依赖查找 ============
# OpenCV：通过系统环境变量 OpenCV_DIR 自动定位（不再硬编码路径）
# 说明：OpenCV 4.12 预编译库为 vc16（MSVC2019 工具集）；当前 MSVC 19.51（VS2026）
#       太新，OpenCVConfig.cmake 无法自动识别，故手动指定运行库，借助 MSVC 二进制兼容链接。
if(NOT DEFINED OpenCV_ARCH)
    set(OpenCV_ARCH x64)
endif()
if(NOT DEFINED OpenCV_RUNTIME)
    set(OpenCV_RUNTIME vc16)
endif()
find_package(OpenCV REQUIRED)

# ============ 头文件搜索目录 ============
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3)  # sqlite3.h

# ============ 源文件 ============
# 自动收集 src 目录下所有 .cpp 文件
file(GLOB_RECURSE WAKEAI_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# ============ 生成可执行程序 ============
add_executable(WakeAI ${WAKEAI_SOURCES})

# 链接 OpenCV 库 + SQLite 库
target_link_libraries(WakeAI ${OpenCV_LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3/sqlite3.lib)

# 运行时把 sqlite3.dll 拷到 exe 旁
if(WIN32)
    add_custom_command(TARGET WakeAI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3/sqlite3.dll
                $<TARGET_FILE_DIR:WakeAI>)
endif()

# ============ 打印信息，方便确认是否找到 OpenCV ============
message(STATUS "OpenCV 版本: ${OpenCV_VERSION}")
message(STATUS "OpenCV 库文件: ${OpenCV_LIBS}")
