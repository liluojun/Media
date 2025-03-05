//
// Created by Administrator on 2020/3/21.
//

#ifndef PLAYVIDEO_FFMPEGENCODESTREAM_H
#define PLAYVIDEO_FFMPEGENCODESTREAM_
#ifdef __cplusplus
extern "C" {
#include "FrameCallback.h"
#include "native_log.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#endif
class FFmpegEncodeStream {
public:
    int openStream(char *path);
    void setFrameCallback(FrameCallback *callback);
    ~FFmpegEncodeStream();
};

#ifdef __cplusplus
}
#endif
#endif //PLAYVIDEO_FFMPEGENCODESTREAM_H
