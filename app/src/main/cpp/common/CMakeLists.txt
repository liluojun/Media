
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("common")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 根据当前 ABI 动态获取预编译库路径
set(LOCAL_PATH ${CMAKE_CURRENT_SOURCE_DIR})  # 等效于 $(call my-dir)

set(MEDIE_SOURCES
        src/YuvData.cpp
        src/cJSON.c
        src/Utils.cpp
        src/stb_image_write.cpp
)
include_directories("${LOCAL_PATH}/include")
add_library(${PROJECT_NAME} SHARED ${MEDIE_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE
        log
        android
)