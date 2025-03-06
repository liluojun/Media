
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("medie")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 根据当前 ABI 动态获取预编译库路径
set(LOCAL_PATH ${CMAKE_CURRENT_SOURCE_DIR})  # 等效于 $(call my-dir)
set(YUV_LIB_DIR "${LOCAL_PATH}/ffmpeg/${ANDROID_ABI}")
set(MEDIE_SOURCES
        native.cpp
        JniUtils.cpp
        FFmpegEncodeStream.cpp
        AiLineHelper.cpp
        VideoRender.cpp
        EglEnvironment.cpp
        EglUtils.cpp
        GlRendering.cpp
        GlDraw.cpp
        GlDrawAi.cpp
        GlThread.cpp
        Looper.cpp
        Utils.cpp
        MediaController.cpp

)
# 添加预编译的 so
add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES
        IMPORTED_LOCATION "${YUV_LIB_DIR}/lib/libavcodec.so"
)
add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter PROPERTIES
        IMPORTED_LOCATION "${YUV_LIB_DIR}/lib/libavfilter.so"
)
add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES
        IMPORTED_LOCATION "${YUV_LIB_DIR}/lib/libavformat.so"
)
add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES
        IMPORTED_LOCATION "${YUV_LIB_DIR}/lib/libavutil.so"
)
add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES
        IMPORTED_LOCATION "${YUV_LIB_DIR}/lib/libswresample.so"
)
add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES
        IMPORTED_LOCATION "${YUV_LIB_DIR}/lib/libswscale.so"
)
include_directories("${YUV_LIB_DIR}/include")
add_library(${PROJECT_NAME} SHARED ${MEDIE_SOURCES})

# 链接依赖库
target_link_libraries(${PROJECT_NAME} PRIVATE
        avformat
        avcodec
        avfilter
        avutil
        swresample
        swscale # 预编译库
        android
        log
        GLESv2
        EGL
)
