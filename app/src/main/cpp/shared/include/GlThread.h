//
// Created by hjt on 2024/2/19.
//

#ifndef PLAYVIDEO_GLTHREAD_H
#define PLAYVIDEO_GLTHREAD_H


#include "Looper.h"
#include "VideoRender.h"
#include "../../common/include/YuvData.h"
#include "AiLineHelper.h"

enum {
    kMsgSurfaceCreated,
    kMsgSurfaceChanged,
    kMsgSurfaceDestroyed,
    kMsgYuvData,
    kMsgAiFrame,
    kMsgShareModel,
    kMsgScreenShot,
};
typedef struct AiFrame {
    long timestamp;
    AiLineData *data;
};
typedef struct ScreenShot {
    long timestamp;
    std::string *imagePath;
} ;
class GlThread : public Looper {

public:
    GlThread();

    virtual ~GlThread();

    virtual void handleMessage(LooperMessage *msg);

    bool getIsSurfaceCreated();

private:
    VideoRender *mRender;
    AiFrame *mAiFrame;
    ScreenShot *mScreenShot;
    int shareModel=1;//0填充模式，1等比缩放，2居中填充。

    void drawVideoFrames(RenderWindow *m, YuvData *pData, int arg1, int arg2);

    bool drawAiFrames(RenderWindow *m, int w, int h);
    void drawFboMix(RenderWindow *m, int w, int h);
    void calculateScale(float contentW, float contentH, float viewW, float viewH, int fit, float outScale[2]);
};


#endif