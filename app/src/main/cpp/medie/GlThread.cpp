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
                if (mRender->m != NULL) {
                    glClearColor(1.0, 1.0, 1.0, 1.0);
                    glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    drawVideoFrames(mRender->m, (YuvData *) msg->obj, msg->arg1,
                                    msg->arg2);

                    bool result = drawAiFrames(mRender->m, msg->arg1,
                                               msg->arg2);
                    if (result)
                        drawFboMix(mRender->m, msg->arg1,
                                   msg->arg2);

                    mRender->m->mEglEnvironment->swapBuffers();


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
    delete (data);
    m->mGlDraw->drawYuv(yuvTextures, 0, 0, m->w, m->h);
}

bool GlThread::drawAiFrames(RenderWindow *m, int w, int h) {
    if (mAiFrame != nullptr && mAiFrame->data != nullptr) {
        if (getTimestampMillis() - mAiFrame->timestamp >10000) {
            delete (mAiFrame->data);
            mAiFrame->data = nullptr;
            mAiFrame->timestamp = 0;
            return false;
        } else {
            m->mGlDrawAi->drawAi(mAiFrame->data, w, h, m->w, m->h);
            return true;
        }

    } else{
        return false;
    }
}

void GlThread::drawFboMix(RenderWindow *m, int w, int h) {
    m->mGlDrawFbo->draw(m->mGlDrawAi->getFboTexture(), w, h,m->w, m->h);
}

bool GlThread::getIsSurfaceCreated() {
    if (mRender)
        return mRender->isSurfaceCreated;
    else
        return false;
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




