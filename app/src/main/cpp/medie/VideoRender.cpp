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
        ANativeWindow_release(m->mWindow);
        m->mEglEnvironment->releaseSurface(m->mEGLSurface);
        m->mGlDraw->release();
        m->mGlDrawAi->release();
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

#endif
#ifdef __cplusplus


#endif