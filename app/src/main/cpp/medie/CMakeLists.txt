
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
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 根据当前 ABI 动态获取预编译库路径
set(LOCAL_PATH ${CMAKE_CURRENT_SOURCE_DIR})  # 等效于 $(call my-dir)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../jniLibs/${ANDROID_ABI})
set(OBOE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../oboe)
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
set(DECODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../decode)
set(MEDIE_SOURCES
        native.cpp
        JniUtils.cpp
        MediaController.cpp
)
add_subdirectory(${OBOE_DIR} ./oboe)
add_subdirectory(${COMMON_DIR} ./common)
add_subdirectory(${SHARED_DIR} ./shared)
add_subdirectory(${DECODE_DIR} ./decode)
add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES
        IMPORTED_LOCATION "${DECODE_DIR}/ffmpeg/${ANDROID_ABI}/lib/libavcodec.so"
)
add_library(${PROJECT_NAME} SHARED ${MEDIE_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
        ${OBOE_DIR}/include
        ${COMMON_DIR}/include
        ${SHARED_DIR}/include
        ${DECODE_DIR}/include
        ${DECODE_DIR}/ffmpeg/include   # 加这一行
)
# 链接依赖库
target_link_libraries(${PROJECT_NAME} PRIVATE
        android
        oboe
        common
        shared
        decode
        avcodec
)
