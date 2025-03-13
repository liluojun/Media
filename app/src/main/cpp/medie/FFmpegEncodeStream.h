//
// Created by Administrator on 2020/3/21.
//

#ifndef PLAYVIDEO_FFMPEGENCODESTREAM_H
#define PLAYVIDEO_FFMPEGENCODESTREAM_H

#ifdef __cplusplus
#include <chrono>
#include <thread>
#include <queue>
#include <pthread.h>

extern "C" {
#include "FrameCallback.h"
#include "native_log.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"

#endif
struct AudioData {
    uint8_t *data;
    int size;
    double pts;

    AudioData() : data(nullptr), size(0), pts(0.0) {}

    ~AudioData() { if (data) av_free(data); }
};
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
    std::queue<AVFrame *> videoQueue;
    std::queue<AudioData> audioQueue;
    pthread_mutex_t videoMutex;
    pthread_mutex_t audioMutex;
    pthread_cond_t videoCond;
    pthread_cond_t audioCond;
    double audioClock;
    pthread_mutex_t audioClockMutex;
    bool abortPlayback;
    pthread_t videoThread;
    pthread_t audioThread;

    ~DecodeContext() {
        cleanup();
    }

    void init() {
        pthread_mutex_init(&videoMutex, nullptr);
        pthread_mutex_init(&audioMutex, nullptr);
        pthread_cond_init(&videoCond, nullptr);
        pthread_cond_init(&audioCond, nullptr);
        pthread_mutex_init(&audioClockMutex, nullptr);
        abortPlayback = false;
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
        pthread_mutex_destroy(&videoMutex);
        pthread_mutex_destroy(&audioMutex);
        pthread_cond_destroy(&videoCond);
        pthread_cond_destroy(&audioCond);
        pthread_mutex_destroy(&audioClockMutex);

        while (!videoQueue.empty()) {
            av_frame_free(&videoQueue.front());
            videoQueue.pop();
        }
        while (!audioQueue.empty()) {
            audioQueue.pop();
        }
    }
} DecodeContext;
class FFmpegEncodeStream {
private:
    DecodeContext *decodeCtx;
    pthread_t threadId;
public:
    bool containsSEI(const uint8_t *data, int size);

    bool extractSEIData(const uint8_t *data, int size, uint8_t **seiData, int *seiSize);

    FFmpegEncodeStream();

    ~FFmpegEncodeStream();

    bool openStream(const char *path);

    void setFrameCallback(FrameCallback *callback);

    void setAudioCallback(AudioCallback *callback);

    void closeStream();
};

#ifdef __cplusplus
}
#endif
#endif //PLAYVIDEO_FFMPEGENCODESTREAM_H
