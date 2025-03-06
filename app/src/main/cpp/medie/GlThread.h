//
// Created by hjt on 2024/2/19.
//

#ifndef PLAYVIDEO_GLTHREAD_H
#define PLAYVIDEO_GLTHREAD_H


#include "Looper.h"
#include "VideoRender.h"
#include "YuvData.h"
#include "AiLineHelper.h"

enum {
    kMsgSurfaceCreated,
    kMsgSurfaceChanged,
    kMsgSurfaceDestroyed,
    kMsgYuvData,
    kMsgAiFrame,
};
typedef struct AiFrame {
    long timestamp;
    AiLineData *data;
};

class GlThread : public Looper {

public:
    GlThread();

    virtual ~GlThread();

    virtual void handleMessage(LooperMessage *msg);

    bool getIsSurfaceCreated();

private:
    VideoRender *mRender;
    AiFrame *mAiFrame;

    void drawVideoFrames(RenderWindow *m, YuvData *pData, int arg1, int arg2);

    void drawAiFrames(RenderWindow *m);
};


#endif