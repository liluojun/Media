//
// Created by hjt on 2025/7/2.
//
//DecodeCommonUtils
#ifndef MEDIA_DECODECOMMONUTILS_H
#define MEDIA_DECODECOMMONUTILS_H
extern "C" {
#include "../ffmpeg/include/libavutil/frame.h"
}

#include "../yuv/include/libyuv.h"
#include <cstring> // for memset

class DecodeCommonUtils {
public:
    // 将两平面NV12/NV21的AVFrame转换成三平面YUV420P的AVFrame
    // 返回新分配的YUV420P AVFrame，失败返回nullptr
    static AVFrame *ConvertToYUV420PWithLibyuv(const AVFrame *src);
};

#endif //MEDIA_DECODECOMMONUTILS_H
