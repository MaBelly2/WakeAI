cmake_minimum_required(VERSION 3.16)
project(WakeAI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt：Widgets + Multimedia(响铃/摄像头显示) + Sql(数据库)
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets Multimedia Sql)
if(NOT Qt6_FOUND)
    message(FATAL_ERROR "找不到 Qt6！请先装 Qt 6 并给当前 Kit 设 Qt6，或指定 CMAKE_PREFIX_PATH。")
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Sql)
find_package(OpenCV REQUIRED)   # 前端 motionworker 直接用了 OpenCV

add_subdirectory(wakeai)        # 引入 wakeai_core + WakeAI_system

if(MSVC)
    add_compile_options(/utf-8)   # 整个工程都用 UTF-8 读源码，消除 C4828
endif()

qt_add_executable(alarm_ui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    motionworker.cpp
    motionworker.h
    ringtonedialog.cpp
    ringtonedialog.h
    ringtonemanager.cpp
    ringtonemanager.h
    uiwidgets.cpp
    uiwidgets.h
    alarmcardwidget.cpp          # ← 必须补，之前漏了
    alarmcardwidget.h
    resources.qrc
)

target_link_libraries(alarm_ui PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Sql
    wakeai_core
    WakeAI_system
    ${OpenCV_LIBS}
)

# 让程序从源码 models 直接读模型
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/models" WAKEAI_SOURCE_MODEL_DIR)
target_compile_definitions(alarm_ui PRIVATE
    WAKEAI_SOURCE_MODEL_DIR="${WAKEAI_SOURCE_MODEL_DIR}")
set_target_properties(alarm_ui PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

# 部署：拷 Qt DLL + 模型（适配没装 Qt 的电脑）
if(WIN32)
    find_program(WAKEAI_WINDEPLOYQT windeployqt
        HINTS "${Qt6_DIR}/../../../bin" NO_DEFAULT_PATH REQUIRED)
    add_custom_command(TARGET alarm_ui POST_BUILD
        COMMAND "${WAKEAI_WINDEPLOYQT}"
            "$<IF:$<CONFIG:Debug>,--debug,--release>" --no-compiler-runtime
            "$<TARGET_FILE:alarm_ui>" VERBATIM)
    add_custom_command(TARGET alarm_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:alarm_ui>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/models/yolov8n-pose.onnx"
                "$<TARGET_FILE_DIR:alarm_ui>/models/yolov8n-pose.onnx")
endif()