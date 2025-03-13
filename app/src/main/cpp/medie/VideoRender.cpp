//
// Created by hjt on 2024/1/26.
//

#include "VideoRender.h"

using namespace std;
#ifdef __cplusplus

VideoRender::VideoRender() {

}

long VideoRender::creatSurface(ANativeWindow *mWindow, jint w, jint h) {
    m = new RenderWindow();
    m->mWindow = mWindow;
    m->mEglEnvironment = new EglEnvironment();
    m->mEGLSurface = m->mEglEnvironment->createWindowSurface(m->mWindow);
    m->mGlDraw = new GlDraw();
    m->mGlDrawAi = new GlDrawAi();
    m->mGlDrawFbo = new GlDrawFbo();
    m->handel = -1;
    m->h = h;
    m->w = w;
    isSurfaceCreated = true;
    return m->handel;
}

int VideoRender::destorySurface() {
    if (NULL != m) {
        isSurfaceCreated = false;
        if (m->mEglEnvironment) {
            m->mEglEnvironment->releaseSurface(m->mEGLSurface);
        }

        // 2. 释放Native窗口（需确保生命周期管理）
        if (m->mWindow) {
            ANativeWindow_release(m->mWindow);
            m->mWindow = nullptr;
        }

        // 3. 释放图形对象
        if (m->mGlDraw) {
            m->mGlDraw->release();
            delete m->mGlDraw;
        }
        if (m->mGlDrawAi) {
            m->mGlDrawAi->release();
            delete m->mGlDrawAi;
        }
        if (m->mGlDrawFbo) {
            m->mGlDrawFbo->release();
            delete m->mGlDrawFbo;
        }
        delete m;
        return 0;
    } else {
        return -1;
    }
}

int VideoRender::changeSurfaceSize(int w, int h) {

    if (NULL != m) {
        m->h = h;
        m->w = w;
        m->mEglEnvironment->changeSurfaceSize(w, h);
        return 0;
    } else {
        return -1;
    }
}

VideoRender::~VideoRender() {
    if (NULL != m) {
        delete m;
    }

}

#endif
#ifdef __cplusplus


#endif