//
// Created by Administrator on 2025/6/30.
//
#include "../include/SurfaceHolder.h"

SurfaceHolder::SurfaceHolder(JavaVM *vm, JNIEnv *env, jobject surface) : javaVm(vm) {
    if (surface && env) {
        surfaceGlobalRef = env->NewGlobalRef(surface);  // 保存全局引用
    }
}

SurfaceHolder::~SurfaceHolder() {
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
    if (surfaceGlobalRef) {
        JNIEnv *env = nullptr;
        if (javaVm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK) {
            env->DeleteGlobalRef(surfaceGlobalRef);
        } else if (javaVm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            env->DeleteGlobalRef(surfaceGlobalRef);
            javaVm->DetachCurrentThread();
        }
        surfaceGlobalRef = nullptr;
    }
}

ANativeWindow *SurfaceHolder::getNativeWindow() {
    if (nativeWindow == nullptr && surfaceGlobalRef != nullptr) {
        JNIEnv *env = nullptr;
        bool needDetach = false;
        if (javaVm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (javaVm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                needDetach = true;
            } else {
                return nullptr;
            }
        }
        nativeWindow = ANativeWindow_fromSurface(env, surfaceGlobalRef);
        if (needDetach) {
            javaVm->DetachCurrentThread();
        }
    }
    return nativeWindow;
}

void SurfaceHolder::releaseNativeWindow() {
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
}

jobject SurfaceHolder::getGlobalRef() {
    return surfaceGlobalRef;
}
