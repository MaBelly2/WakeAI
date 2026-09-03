cmake_minimum_required(VERSION 3.16)
project(WakeAI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 需要 moc 预处理，必须开启
set(CMAKE_AUTOMOC ON)

# MSVC 中文编码
if (MSVC)
    add_compile_options(/utf-8)
endif()

# ============ OpenCV 依赖 ============
# 说明：OpenCV 4.12 预编译库为 vc16（MSVC2019 工具集）；当前 MSVC 19.51（VS2026）
#       太新，OpenCVConfig.cmake 无法自动识别，故手动指定运行库，借助 MSVC 二进制兼容链接。
if(NOT DEFINED OpenCV_ARCH)
    set(OpenCV_ARCH x64)
endif()
if(NOT DEFINED OpenCV_RUNTIME)
    set(OpenCV_RUNTIME vc16)
endif()
find_package(OpenCV REQUIRED)

# Qt6：Widgets（UI）+ Sql（数据库）
find_package(Qt6 COMPONENTS Widgets Sql REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

message(STATUS "OpenCV 版本: ${OpenCV_VERSION}")
message(STATUS "Qt 版本: ${Qt6_VERSION}")

# ============ 核心算法库（Qt 程序和测试工具共用） ============
file(GLOB WAKEAI_CORE_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/exercise/*.cpp)

add_library(WakeAI_core STATIC ${WAKEAI_CORE_SOURCES})
target_link_libraries(WakeAI_core PUBLIC ${OpenCV_LIBS})

# ============ 系统数据层库（数据库/闹钟/成就，D 负责人） ============
file(GLOB WAKEAI_SYSTEM_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/system/*.cpp)

add_library(WakeAI_system STATIC ${WAKEAI_SYSTEM_SOURCES})
target_link_libraries(WakeAI_system PUBLIC Qt6::Core Qt6::Sql)

# ============ 正式 Qt 程序 ============
file(GLOB WAKEAI_APP_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp)

add_executable(WakeAI
    ${WAKEAI_APP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/MainWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/system/AlarmManager.h)
target_link_libraries(WakeAI PRIVATE WakeAI_core WakeAI_system Qt6::Widgets)

# ============ 命令行测试工具（原来的 main.cpp） ============
add_executable(WakeAI_test tools/live_test.cpp)
target_link_libraries(WakeAI_test PRIVATE WakeAI_core)

# ============ 系统模块自测（D 负责人） ============
add_executable(WakeAITest
    tests/test_system.cpp
    src/system/DatabaseManager.cpp
    src/system/AlarmManager.cpp
    src/system/AchievementEngine.cpp
    include/system/AlarmManager.h)
target_link_libraries(WakeAITest Qt6::Core Qt6::Sql)

# ============ 系统模块成果可视化（D 负责人，临时查看用） ============
add_executable(WakeAIViewer
    tools/system_viewer.cpp
    src/system/DatabaseManager.cpp
    src/system/AchievementEngine.cpp)
target_link_libraries(WakeAIViewer Qt6::Core Qt6::Sql Qt6::Widgets)
