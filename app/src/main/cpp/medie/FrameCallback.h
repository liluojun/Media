

//
// Created by hjt on 2025/3/4.
//
#ifndef FRAME_CALLBACK_H
#define FRAME_CALLBACK_H

#include <stdint.h>
#include "ffmpeg/include/libavformat/avformat.h"

#ifdef __cplusplus
extern "C" {
#endif
class FrameCallback {
public:
    virtual ~FrameCallback() = default;

    virtual void onFrameEncoded(AVFrame *mAVFrame) = 0;
};
class AudioCallback {
public:
    virtual ~AudioCallback() = default;

    virtual void onAudioEncoded(uint8_t *mData,int size) = 0;
};
#ifdef __cplusplus
}
#endif
#endif