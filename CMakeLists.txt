cmake_minimum_required(VERSION 3.16)
project(WakeAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt6 元对象编译器（AlarmManager 等 QObject 类需要）
set(CMAKE_AUTOMOC ON)

# MSVC 默认按系统代码页(GBK/936)读源码，中文注释会乱码；强制按 UTF-8 读取
if (MSVC)
    add_compile_options(/utf-8)
endif()

# ============ 依赖查找 ============
# Qt6：Core（QTimer）、Sql（SQLite 数据库）；铃声第一版用系统 Beep，无需 Multimedia
# 配置时通过 -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/msvc2022_64 指定 Qt 位置
find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets)

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

# ============ 源文件 ============
# 自动收集 src 目录下所有 .cpp 文件
file(GLOB_RECURSE WAKEAI_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
# 头文件也加入 target，便于 AUTOMOC 处理 Q_OBJECT（AlarmManager 等）
file(GLOB_RECURSE WAKEAI_HEADERS
     ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

# ============ 生成可执行程序 ============
add_executable(WakeAI ${WAKEAI_SOURCES} ${WAKEAI_HEADERS})

# 链接 Qt6 + OpenCV
target_link_libraries(WakeAI
    Qt6::Core
    Qt6::Sql
    ${OpenCV_LIBS})

# ============ 打印信息，方便确认依赖 ============
message(STATUS "OpenCV 版本: ${OpenCV_VERSION}")
message(STATUS "Qt6 版本: ${Qt6_VERSION}")

# ============ 系统模块自测（D 负责人）============
add_executable(WakeAITest
    tests/test_system.cpp
    src/system/DatabaseManager.cpp
    src/system/AlarmManager.cpp
    src/system/AchievementEngine.cpp
    include/system/AlarmManager.h)
target_link_libraries(WakeAITest Qt6::Core Qt6::Sql)

# ============ 系统模块成果可视化（D 负责人，临时查看用）============
add_executable(WakeAIViewer
    tools/system_viewer.cpp
    src/system/DatabaseManager.cpp
    src/system/AchievementEngine.cpp)
target_link_libraries(WakeAIViewer Qt6::Core Qt6::Sql Qt6::Widgets)
