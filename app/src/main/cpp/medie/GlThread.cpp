//
// Created by hjt on 2024/2/19.
//

#include "GlThread.h"
#include "AiLineHelper.h"
#include "Utils.cpp"

GlThread::GlThread() {
    mRender = new VideoRender();
    mAiFrame = new AiFrame();
    AiLineHelper *mAiLineHelper = new AiLineHelper();
    mAiFrame->data = new AiLineData();
    mAiLineHelper->creatAiLineData(mAiFrame->data);
    mAiFrame->timestamp = getTimestampMillis();

}


void GlThread::handleMessage(LooperMessage *msg) {
    if (!msg) {
        return;
    }
    switch (msg->what) {
        case kMsgShareModel: {
            shareModel = msg->arg1;
        }
        case kMsgSurfaceCreated:
            if (mRender) {
                mRender->creatSurface((ANativeWindow *) msg->obj, msg->arg1, msg->arg2);
            }
            break;

        case kMsgSurfaceChanged:
            if (mRender) {
                mRender->changeSurfaceSize(msg->arg1, msg->arg2);
            }
            break;

        case kMsgSurfaceDestroyed:
            if (mRender) {
                mRender->destorySurface();
            }
            break;
        case kMsgYuvData:
            if (mRender) {
                if (mRender->m != NULL && mRender->isSurfaceCreated) {
                    glClearColor(0.0, 0.0, 0.0, 1.0);
                    glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    drawVideoFrames(mRender->m, (YuvData *) msg->obj, msg->arg1,
                                    msg->arg2);
                    bool result = drawAiFrames(mRender->m, msg->arg1,
                                               msg->arg2);
                    if (result)
                        drawFboMix(mRender->m, msg->arg1,
                                   msg->arg2);
                    mRender->m->mEglEnvironment->swapBuffers();

                    delete (msg->obj);
                }
            }
            break;
        case kMsgAiFrame: {
            if (mAiFrame) {
                mAiFrame->data = (AiLineData *) msg->obj;
                mAiFrame->timestamp = getTimestampMillis();
            }
        }
    }
}

void GlThread::drawVideoFrames(RenderWindow *m, YuvData *data, int w, int h) {
    GLuint yuvTextures[3];
    m->mGlDraw->perparDrawYuv(w, h, data, yuvTextures);
    float scale[2]={1.0f,1.0f};
    calculateScale(w, h, m->w, m->h, shareModel, scale);
    m->mGlDraw->drawYuv(yuvTextures, 0, 0, m->w, m->h,  scale);
}

bool GlThread::drawAiFrames(RenderWindow *m, int w, int h) {
    if (mAiFrame != nullptr && mAiFrame->data != nullptr) {
        if (getTimestampMillis() - mAiFrame->timestamp > 10000) {
            delete (mAiFrame->data);
            mAiFrame->data = nullptr;
            mAiFrame->timestamp = 0;
            return false;
        } else {
            float scale[2]={1.0f,1.0f};
            calculateScale(w, h, m->w, m->h, shareModel, scale);
            m->mGlDrawAi->drawAi(mAiFrame->data, w, h, m->w, m->h,scale);
            return true;
        }

    } else {
        return false;
    }
}

void GlThread::drawFboMix(RenderWindow *m, int w, int h) {
    m->mGlDrawFbo->draw(m->mGlDrawAi->getFboTexture(), w, h, m->w, m->h);
}

bool GlThread::getIsSurfaceCreated() {
    if (mRender)
        return mRender->isSurfaceCreated;
    else
        return false;
}

void GlThread::calculateScale(float contentW, float contentH, float viewW, float viewH, int fit,
                    float outScale[2]) {
    float contentRatio = contentW / contentH;
    float viewRatio = viewW / viewH;

    float scaleX, scaleY;

    if (fit == 1) {
        if (contentRatio > viewRatio) {
            scaleX = 1.0f;
            scaleY = viewRatio / contentRatio;
        } else {
            scaleX = contentRatio / viewRatio;
            scaleY = 1.0f;
        }
    } else if (fit == 2) {
        if (contentRatio > viewRatio) {
            scaleX = contentRatio / viewRatio;
            scaleY = 1.0f;
        } else {
            scaleX = 1.0f;
            scaleY = viewRatio / contentRatio;
        }
    } else {
        scaleX = 1.0f;
        scaleY = 1.0f;
    }


    outScale[0] = scaleX;
    outScale[1] = scaleY;
}

GlThread::~GlThread() {
    if (mRender) {
        delete mRender;
        mRender = nullptr;  // 避免野指针
    }
    if (mAiFrame) {
        delete mAiFrame;
        mAiFrame = nullptr;
    }
}




