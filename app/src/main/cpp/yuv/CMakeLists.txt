
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)
set(ANDROID_NDK G:/sdk/ndk/25.1.8937393)
set(CMAKE_TOOLCHAIN_FILE ${ANDROID_NDK}/build/cmake/android.toolchain.cmake)
# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("yuv")

# ------------------------- 预编译库 yuvl 配置 -------------------------
# 根据当前 ABI 动态获取预编译库路径
set(LOCAL_PATH ${CMAKE_CURRENT_SOURCE_DIR})  # 等效于 $(call my-dir)
set(YUV_LIB_DIR "${LOCAL_PATH}/localfile/${ANDROID_ABI}")

# 添加预编译的 libyuv.so
add_library(yuvl SHARED IMPORTED)
set_target_properties(yuvl PROPERTIES
        IMPORTED_LOCATION "${YUV_LIB_DIR}/libyuv.so"
        # 可选：若需要指定 API 兼容性
        # VERSION 1.0.0
        # SOVERSION 1
)

# ------------------------- 主模块 mediayuv 配置 -------------------------
# 添加共享库 mediayuv
include_directories(${LOCAL_PATH})
add_library(${PROJECT_NAME}  SHARED
        media_jni_NativeMethod.cpp
)

# 链接依赖库
target_link_libraries(${PROJECT_NAME}  PRIVATE
        yuvl  # 预编译库
        android
        log    # Android log 库
)
