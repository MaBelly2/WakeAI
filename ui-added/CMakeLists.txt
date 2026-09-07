# Configure from the repository root.
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
qt_add_executable(alarm_ui main.cpp mainwindow.cpp mainwindow.h mainwindow.ui
    motionworker.cpp motionworker.h resources.qrc)
target_link_libraries(alarm_ui PRIVATE
    wakeai_core WakeAI_system Qt6::Widgets Qt6::Multimedia)
# Development reads the single model in the source repository, without copying.
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}/models" WAKEAI_SOURCE_MODEL_DIR)
target_compile_definitions(alarm_ui PRIVATE
    WAKEAI_SOURCE_MODEL_DIR="${WAKEAI_SOURCE_MODEL_DIR}")
set_target_properties(alarm_ui PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

if(WIN32)
    find_program(WAKEAI_WINDEPLOYQT windeployqt
        HINTS "${Qt6_DIR}/../../../bin" NO_DEFAULT_PATH REQUIRED)
    add_custom_command(TARGET alarm_ui POST_BUILD
        COMMAND "${WAKEAI_WINDEPLOYQT}"
            "$<IF:$<CONFIG:Debug>,--debug,--release>" --no-compiler-runtime
            "$<TARGET_FILE:alarm_ui>" VERBATIM)
    set(WAKEAI_OPENCV_RUNTIME_DIR "" CACHE PATH "OpenCV Windows runtime DLL directory")
    if(WAKEAI_OPENCV_RUNTIME_DIR)
        file(GLOB WAKEAI_OPENCV_DLLS "${WAKEAI_OPENCV_RUNTIME_DIR}/*.dll")
        # Development folder includes both configurations for F5/Debug switching.
        foreach(dll IN LISTS WAKEAI_OPENCV_DLLS)
            add_custom_command(TARGET alarm_ui POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${dll}" "$<TARGET_FILE_DIR:alarm_ui>" VERBATIM)
        endforeach()
    endif()
endif()
