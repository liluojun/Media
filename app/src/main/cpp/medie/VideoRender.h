//
// Created by hjt on 2024/1/26.
//

#include <jni.h>
/* Header for class media_jni_MediaNative */
#ifndef _Included_VideoRender
#define _Included_VideoRender
#ifdef __cplusplus

#include "native_log.h"
#include <iostream>
#include <map>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "EglEnvironment.h"
#include "GlRendering.h"
#include "GlDraw.h"
#include "GlDrawAi.h"
#include "GlDrawFbo.h"

using namespace std;

#endif
typedef struct RenderWindow {
    ANativeWindow *mWindow = NULL;
    long handel = 0;
    int w = 0;
    int h = 0;
    EGLSurface mEGLSurface = NULL;
    GlDraw *mGlDraw = NULL;
    GlDrawAi *mGlDrawAi = NULL;
    GlDrawFbo *mGlDrawFbo = NULL;
    EglEnvironment *mEglEnvironment = NULL;

    ~RenderWindow() {
        // 1. 释放EGL资源
        if (mEglEnvironment) {
            if (mEGLSurface != EGL_NO_SURFACE) {
                mEglEnvironment->releaseSurface(mEGLSurface);
            }
            delete mEglEnvironment; // 需要确保 EglEnvironment 自身管理EGLDisplay
        }

        // 2. 释放Native窗口（需确保生命周期管理）
        if (mWindow) {
            ANativeWindow_release(mWindow);
            mWindow = nullptr;
        }

        // 3. 释放图形对象
        delete mGlDraw;
        delete mGlDrawAi;
        delete mGlDrawFbo;
    }
};

class VideoRender {
public:
    VideoRender();

    ~VideoRender();

    long creatSurface(ANativeWindow *mWindow, int w, int h);

    int destorySurface();

    int changeSurfaceSize(int w, int h);

    RenderWindow *m;
    bool isSurfaceCreated;
};

#ifdef __cplusplus

#endif
#endif

