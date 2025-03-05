//
// Created by cain on 2017/12/28.
//

#ifndef CAINCAMERA_NATIVE_LOG_H
#define CAINCAMERA_NATIVE_LOG_H

#include <android/log.h>

#define JNI_DEBUG 1
#define JNI_TAG "ffmpeg"

#define LOGE(format, ...) if (JNI_DEBUG) { __android_log_print(ANDROID_LOG_ERROR, JNI_TAG, format, ##__VA_ARGS__); }
#define LOGI(format, ...) if (JNI_DEBUG) { __android_log_print(ANDROID_LOG_INFO,  JNI_TAG, format, ##__VA_ARGS__); }
#define LOGD(format, ...) if (JNI_DEBUG) { __android_log_print(ANDROID_LOG_DEBUG, JNI_TAG, format, ##__VA_ARGS__); }
#define LOGW(format, ...) if (JNI_DEBUG) { __android_log_print(ANDROID_LOG_WARN,  JNI_TAG, format, ##__VA_ARGS__); }
#define ERROR_CODE_TO_INT(code) static_cast<int>(code)
enum class ErrorCode {
    SUCCESS = 0,
    ERROR_UNKNOWN = -1,//一般性错误
    PATH_ALREADY_EXIST = -2,//该视频流已存在
    PLAYER_NOT_EXIST = -3,//player对象不存在

};
#endif //CAINCAMERA_NATIVE_LOG_H
