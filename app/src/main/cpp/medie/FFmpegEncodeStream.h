//
// Created by Administrator on 2020/3/21.
//

#ifndef PLAYVIDEO_FFMPEGENCODESTREAM_H
#define PLAYVIDEO_FFMPEGENCODESTREAM_H


#ifdef __cplusplus
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "native_log.h"
#endif
class FFmpegEncodeStream {
public:
    int openStream(char *path);

    ~FFmpegEncodeStream();

public:
    AVCodec *mAVCodec = NULL;
    AVFrame *mAVFrame = NULL;
    AVPacket *mAVPacket = NULL;
    AVFormatContext *mAVformat = NULL;
    AVCodecContext *mAVCodecCtx = NULL;

};
#ifdef __cplusplus
}
#endif

#endif //PLAYVIDEO_FFMPEGENCODESTREAM_H
