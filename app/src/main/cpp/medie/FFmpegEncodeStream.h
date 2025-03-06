//
// Created by Administrator on 2020/3/21.
//

#ifndef PLAYVIDEO_FFMPEGENCODESTREAM_H
#define PLAYVIDEO_FFMPEGENCODESTREAM_H

#ifdef __cplusplus

extern "C" {
#include "FrameCallback.h"
#include "native_log.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"

#endif
typedef struct DecodeContext {
    AVFormatContext *formatCtx = nullptr;
    AVCodecContext *videoCodecCtx = nullptr;
    AVCodecContext *audioCodecCtx = nullptr;
    SwrContext *swrCtx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    char *filePath = nullptr;
    bool abortRequest = false;
    FrameCallback *frameCallback = nullptr;
    AudioCallback *audioCallback = nullptr;

    ~DecodeContext() {
        cleanup();
    }

    void cleanup() {
        if (swrCtx) {
            swr_free(&swrCtx);
            swrCtx = nullptr;
        }
        if (frame) {
            av_frame_free(&frame);
            frame = nullptr;
        }
        if (packet) {
            av_packet_free(&packet);
            packet = nullptr;
        }
        if (videoCodecCtx) {
            avcodec_free_context(&videoCodecCtx);
            videoCodecCtx = nullptr;
        }
        if (audioCodecCtx) {
            avcodec_free_context(&audioCodecCtx);
            audioCodecCtx = nullptr;
        }
        if (formatCtx) {
            avformat_close_input(&formatCtx);
            formatCtx = nullptr;
        }
    }
} DecodeContext;
class FFmpegEncodeStream {
private: DecodeContext *decodeCtx; pthread_t threadId ;
public:
    FFmpegEncodeStream();
    bool openStream(const char *path);
    void setFrameCallback(FrameCallback *callback);
    void setAudioCallback(AudioCallback *callback);
    void closeStream();
    ~FFmpegEncodeStream();
};

#ifdef __cplusplus
}
#endif
#endif //PLAYVIDEO_FFMPEGENCODESTREAM_H
