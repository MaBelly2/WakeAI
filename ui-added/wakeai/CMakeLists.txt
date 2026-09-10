project(wakeai_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖：OpenCV（你之前设的 OpenCV_DIR 环境变量在这里生效）
find_package(OpenCV REQUIRED)

# 收集所有 .cpp（除了 main.cpp —— 它不在库里面）
file(GLOB_RECURSE WAKEAI_CORE_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(REMOVE_ITEM WAKEAI_CORE_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

# 生成静态库
add_library(wakeai_core STATIC ${WAKEAI_CORE_SOURCES})

# 头文件目录要“传播”给使用方（PUBLIC），你的 Qt 代码才能 #include 它们
target_include_directories(wakeai_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 链接 OpenCV（PUBLIC：用库的人也会带上 OpenCV）
target_link_libraries(wakeai_core PUBLIC ${OpenCV_LIBS})

if (MSVC)
    target_compile_options(wakeai_core PRIVATE /utf-8)
endif()