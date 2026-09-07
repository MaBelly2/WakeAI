cmake_minimum_required(VERSION 3.16)
project(alarm_ui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 的三个自动工具：处理信号槽(moc)、处理 .ui(uic)、处理资源(rcc)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets Multimedia REQUIRED)
find_package(OpenCV REQUIRED)      # 复用你已设好的 OpenCV_DIR

add_subdirectory(wakeai)           # 引入队友库

qt_add_executable(alarm_ui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    motionworker.cpp               # 第 3 部分要新建的两个文件
    motionworker.h
)

target_link_libraries(alarm_ui PRIVATE
    Qt6::Widgets
    Qt6::Multimedia                # 响铃用 QSoundEffect
    wakeai_core                    # 队友库
    ${OpenCV_LIBS}
)

# 每次构建后，把模型和铃声自动复制到 exe 旁边
add_custom_command(TARGET alarm_ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:alarm_ui>/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/yolov8n-pose.onnx
        "$<TARGET_FILE_DIR:alarm_ui>/models/yolov8n-pose.onnx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/alarm.wav
        "$<TARGET_FILE_DIR:alarm_ui>/alarm.wav"
)