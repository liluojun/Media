//
// Created by hjt on 2025/3/3.
//
#include <jni.h>
#include <string>
#include "native.h"
#include "MediaController.h"
#include "native_log.h"
#include "JniUtils.cpp"

#ifdef __cplusplus
extern "C" {
#include "libavcodec/jni.h"
#endif
static const char *lpClassPathName = "com/git/media/NativeMedia";
static const JNINativeMethod nativeMethod[] = {
        // Java中的函数名                            函数签名信息                                         native的函数指针
        {"openStream",        "(Ljava/lang/String;Ljava/lang/Object;)I",                                    (void *) (openStream)},
        {"closeStream",       "(Ljava/lang/String;)I",                                    (void *) (closeStream)},
        {"screenshot",        "(Ljava/lang/String;Ljava/lang/String;)I",                  (void *) (screenshot)},
        {"creatSurface",      "(Ljava/lang/String;Ljava/lang/Object;II)I",                (void *) (creatSurface)},
        {"destorySurface",    "(Ljava/lang/String;)I",                                    (void *) (destorySurface)},
        {"changeSurfaceSize", "(Ljava/lang/String;II)I",                                  (void *) (changeSurfaceSize)},
        {"init",              "()I",                                                      (void *) (init)},
        {"creatM3u8File",     "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void *) (creatM3u8File)},
        {"m3u8ToMp4",         "(Ljava/lang/String;Ljava/lang/String;)I",                  (void *) (m3u8ToMp4)},
        {"playbackSpeed",     "(Ljava/lang/String;D)I",                                   (void *) (playbackSpeed)},
};
MediaController *mediaController = NULL;

// 类库加载时自动调用
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return 0; // 修正错误返回值类型
    }
    av_jni_set_java_vm(vm, nullptr);

    jclass jniClass = env->FindClass(lpClassPathName);
    if (!jniClass) {
        env->ExceptionClear(); // 清除PendingException
        return 0;
    }

    const jint methodCount = sizeof(nativeMethod) / sizeof(JNINativeMethod); // 修正数组长度计算
    if (env->RegisterNatives(jniClass, nativeMethod, methodCount) != JNI_OK) {
        env->DeleteLocalRef(jniClass); // 释放本地引用
        return 0;
    }

    env->DeleteLocalRef(jniClass); // 释放本地引用
    return JNI_VERSION_1_6;
}


JNIEXPORT jint JNICALL init(JNIEnv *env, jobject thiz) {
    mediaController = new MediaController();
    return 0;
}

JNIEXPORT jint JNICALL openStream(JNIEnv *env, jobject thiz, jstring path,jobject surface) {
    jint result = 0;
    if (NULL != mediaController) {
        LOGE("JNIEnv from Java call: %p", env);
        std::string pStr = JStringToStdString(env, path);
        result = mediaController->openStream(&pStr,surface);
    } else {
        LOGE("mediaController is null");
        result = -1;
    }
    return result;
}

JNIEXPORT jint JNICALL
creatSurface(JNIEnv *env, jobject thiz, jstring path, jobject mWindow, jint w, jint h) {
    jint result = 0;
    if (NULL != mediaController) {
        std::string pStr = JStringToStdString(env, path);
        ANativeWindow *mSurface = ANativeWindow_fromSurface(env, mWindow);
        result = mediaController->creatSurface(&pStr, mSurface, w, h);
    } else {
        LOGE("mediaController is null");
        result = -1;
    }
    return result;
}

JNIEXPORT jint JNICALL destorySurface(JNIEnv *env, jobject thiz, jstring path) {
    jint result = 0;
    if (NULL != mediaController) {
        std::string pStr = JStringToStdString(env, path);
        result = mediaController->destorySurface(&pStr);
    } else {
        LOGE("mediaController is null");
        result = -1;
    }
    return result;
}
JNIEXPORT jstring JNICALL creatM3u8File(JNIEnv *env, jobject thiz, jstring path, jstring tsInfo) {
    if (NULL != mediaController) {
        std::string pStr = JStringToStdString(env, path);
        const char *result = mediaController->creatM3u8File(&pStr, jstringTostr(env, tsInfo));
        return env->NewStringUTF(result);
    } else {
        LOGE("mediaController is null");
        return env->NewStringUTF("");
    }

}
JNIEXPORT jint JNICALL m3u8ToMp4(JNIEnv *env, jobject thiz, jstring path, jstring outPath) {
    if (NULL != mediaController) {

        jint result = mediaController->m3u8ToMp4(jstringTostr(env, path),
                                                 jstringTostr(env, outPath));
        return result;
    } else {
        LOGE("mediaController is null");
        return -1;
    }

}
JNIEXPORT jint JNICALL changeSurfaceSize(JNIEnv *env, jobject thiz, jstring path, jint w, jint h) {
    jint result = 0;
    if (NULL != mediaController) {
        std::string pStr = JStringToStdString(env, path);
        result = mediaController->changeSurfaceSize(&pStr, w, h);
    } else {
        LOGE("mediaController is null");
        result = -1;
    }
    return result;
}

JNIEXPORT jint JNICALL closeStream(JNIEnv *env, jobject thiz, jstring path) {
    jint result = 0;
    if (NULL != mediaController) {
        std::string pStr = JStringToStdString(env, path);
        result = mediaController->closeStream(&pStr);
    } else {
        LOGE("mediaController is null");
        result = -1;
    }
    return result;
}
JNIEXPORT jint JNICALL playbackSpeed(JNIEnv *env, jobject thiz, jstring path, jdouble speed) {
    jint result = 0;
    if (NULL != mediaController) {
        std::string pStr = JStringToStdString(env, path);
        result = mediaController->playbackSpeed(&pStr, speed);
    } else {
        LOGE("mediaController is null");
        result = -1;
    }
    return result;
}
JNIEXPORT jint JNICALL screenshot(JNIEnv *env, jobject thiz, jstring path, jstring imagePath) {
    jint result = 0;
    if (NULL != mediaController) {
        std::string pStr = JStringToStdString(env, path);
        std::string imageStr = JStringToStdString(env, imagePath);
        LOGE("screenshot path %s", imageStr.c_str());
        result = mediaController->screenshot(&pStr, &imageStr);
    } else {
        LOGE("mediaController is null");
        result = -1;
    }
    return result;
}
#ifdef __cplusplus
}
#endif