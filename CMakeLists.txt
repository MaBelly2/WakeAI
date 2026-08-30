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

find_package(OpenCV REQUIRED)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

message(STATUS "OpenCV 版本: ${OpenCV_VERSION}")
message(STATUS "Qt 版本: ${Qt6_VERSION}")

# ============ 核心算法库（Qt 程序和测试工具共用） ============
file(GLOB WAKEAI_CORE_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/exercise/*.cpp)

add_library(WakeAI_core STATIC ${WAKEAI_CORE_SOURCES})
target_link_libraries(WakeAI_core PUBLIC ${OpenCV_LIBS})

# ============ 正式 Qt 程序 ============
file(GLOB WAKEAI_APP_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp)

add_executable(WakeAI
    ${WAKEAI_APP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui/MainWindow.h)
target_link_libraries(WakeAI PRIVATE WakeAI_core Qt6::Widgets)

# ============ 命令行测试工具（原来的 main.cpp） ============
add_executable(WakeAI_test tools/live_test.cpp)
target_link_libraries(WakeAI_test PRIVATE WakeAI_core)
