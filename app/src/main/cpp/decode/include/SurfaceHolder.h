//
// Created by Administrator on 2025/6/30.
//

#ifndef MEDIA_SURFACEHOLDER_H
#define MEDIA_SURFACEHOLDER_H
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>

class SurfaceHolder {
public:
    SurfaceHolder(JavaVM *vm, JNIEnv *env, jobject surface);
    ~SurfaceHolder();

    ANativeWindow *getNativeWindow();  // 获取原生窗口
    void releaseNativeWindow();       // 主动释放（可选）

    jobject getGlobalRef();           // 获取原始 GlobalRef
private:
    JavaVM *javaVm = nullptr;
    jobject surfaceGlobalRef = nullptr;
    ANativeWindow *nativeWindow = nullptr;
};

#endif //MEDIA_SURFACEHOLDER_H
