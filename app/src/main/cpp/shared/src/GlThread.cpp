//
// Created by hjt on 2024/2/19.
//

#include "../include/GlThread.h"
#include "../include/AiLineHelper.h"
#include "../../common/src/Utils.cpp"
#include "../../common/include/stb_image_write.h"

GlThread::GlThread() {
    mRender = new VideoRender();
    mAiFrame = new AiFrame();
    AiLineHelper *mAiLineHelper = new AiLineHelper();
    AiLineData *data = new AiLineData();
    mAiLineHelper->creatAiLineData(data);
    (mAiFrame->data).push_back(*data);
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

                    if (mScreenShot != NULL && mScreenShot->imagePath != nullptr && mScreenShot->timestamp + 1000 < getTimestampMillis()) {
                        //截图
                        int viewport[4];
                        glGetIntegerv(GL_VIEWPORT, viewport);
                        int width = viewport[2];
                        int height = viewport[3];
                        std::unique_ptr<uint8_t[]> pixels(new uint8_t[width * height * 4]);
                        glFinish();  // 等待渲染完成
                        LOGE("截图前 glReadPixels");
                        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
                        LOGE("截图前 glReadPixels**");
                        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                        if (status != GL_FRAMEBUFFER_COMPLETE) {
                            LOGE("Framebuffer not complete: 0x%x", status);
                        }
                        // ✅ 马上将 pixels 拷贝交给后台线程
                        std::unique_ptr<uint8_t[]> copy(new uint8_t[width * height * 4]);
                        memcpy(copy.get(), pixels.get(), width * height * 4);

                        std::string path = mScreenShot->imagePath->c_str();  // 拷贝路径，避免 dangling
                        std::thread([pixels = std::move(copy), width, height, path]() {
                            // 翻转 Y + alpha
                            for (int y = 0; y < height / 2; ++y) {
                                for (int x = 0; x < width * 4; ++x) {
                                    std::swap(pixels[y * width * 4 + x],
                                              pixels[(height - 1 - y) * width * 4 + x]);
                                }
                            }
                            for (int i = 0; i < width * height; ++i)
                                pixels[i * 4 + 3] = 255;

                            int success = stbi_write_png(path.c_str(), width, height, 4,
                                                         pixels.get(), width * 4);
                            LOGE("截图前 glReadPixels******   %d  %s", success, path.c_str());
                        }).detach();

                        delete mScreenShot;
                        mScreenShot = nullptr;

                    }
                    mRender->m->mEglEnvironment->swapBuffers();
                    delete (msg->obj);
                }
            }
            break;
        case kMsgAiFrame: {
            if (mAiFrame) {
                AiLineData *pData = static_cast<AiLineData *>(msg->obj);
                (mAiFrame->data).push_back(*pData);
                mAiFrame->timestamp = getTimestampMillis();
                delete (msg->obj);
            }
        }
        case kMsgScreenShot: {
            if (NULL == mScreenShot) {
                mScreenShot = new ScreenShot();
            }
            auto *pathStr = static_cast<std::string *>(msg->obj);
            mScreenShot->imagePath = new std::string(*pathStr);  // ✅ 拷贝内容
            delete pathStr;  // ✅ 释放消息携带的对象
            LOGE("Player screenshot  path %s", mScreenShot->imagePath->c_str());
            mScreenShot->timestamp = getTimestampMillis();

        }
    }
}

void GlThread::drawVideoFrames(RenderWindow *m, YuvData *data, int w, int h) {
    GLuint yuvTextures[3];
    m->mGlDraw->perparDrawYuv(w, h, data, yuvTextures);
    float scale[2] = {1.0f, 1.0f};
    calculateScale(w, h, m->w, m->h, shareModel, scale);
    m->mGlDraw->drawYuv(yuvTextures, 0, 0, m->w, m->h, scale);
}

bool GlThread::drawAiFrames(RenderWindow *m, int w, int h) {
    if (mAiFrame != nullptr && !mAiFrame->data.empty()) {
        if (getTimestampMillis() - mAiFrame->timestamp > 10000) {
            mAiFrame->data.clear();
            mAiFrame->timestamp = 0;
            return false;
        } else {
            float scale[2] = {1.0f, 1.0f};
            calculateScale(w, h, m->w, m->h, shareModel, scale);
            m->mGlDrawAi->drawAi(mAiFrame->data, w, h, m->w, m->h, scale);
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




