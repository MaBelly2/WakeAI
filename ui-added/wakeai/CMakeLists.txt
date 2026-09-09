project(wakeai_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)          # ① 开 moc

find_package(OpenCV REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Sql Core)   # ② 让本目录知道 Qt6，moc 才能找到

file(GLOB WAKEAI_CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp
                              ${CMAKE_CURRENT_SOURCE_DIR}/src/exercise/*.cpp)
add_library(wakeai_core STATIC ${WAKEAI_CORE_SOURCES})
target_include_directories(wakeai_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(wakeai_core PUBLIC ${OpenCV_LIBS})

file(GLOB WAKEAI_SYSTEM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/system/*.cpp)
add_library(WakeAI_system STATIC ${WAKEAI_SYSTEM_SOURCES}
    include/system/AlarmManager.h)   # ③ 显式列出 Q_OBJECT 头文件，保证 moc 处理它
target_include_directories(WakeAI_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(WakeAI_system PUBLIC Qt6::Sql Qt6::Core wakeai_core)

if(MSVC)
    target_compile_options(wakeai_core  PRIVATE /utf-8)
    target_compile_options(WakeAI_system PRIVATE /utf-8)
endif()