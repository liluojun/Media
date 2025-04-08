//
// Created by hjt on 2025/4/8.
//

#ifndef MEDIA_ENCODESTREAM2_H
#define MEDIA_ENCODESTREAM2_H

#ifdef __cplusplus

#include <chrono>
#include <thread>
#include <queue>
#include <pthread.h>
#include "AudioPlayer.h"

extern "C" {
#include "FrameCallback.h"
#include "native_log.h"
#include "ffmpeg/include/libavformat/avformat.h"
#include "ffmpeg/include/libavcodec/avcodec.h"
#include "ffmpeg/include/libswresample/swresample.h"

#endif
struct AudioData {
    uint8_t *data;
    int size;
    double pts;

    AudioData() : data(nullptr), size(0), pts(0.0) {}

    //~AudioData() { if (data) av_free(data); }
};
typedef struct DecodeContext {
    AudioPlayer *audioPlayer = nullptr;
    AVFormatContext *formatCtx = nullptr;
    AVCodecContext *videoCodecCtx = nullptr;
    AVCodecContext *audioCodecCtx = nullptr;
    SwrContext *swrCtx = nullptr;
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    char *filePath = nullptr;
    bool abortRequest = false;
    std::queue<AVPacket *>videoWaitingDecode;
    std::queue<AVPacket *>audioWaitingDecode;
    std::queue<AVFrame *> videoQueue;
    std::queue<AudioData> audioQueue;

    pthread_mutex_t videoMutex;
    pthread_mutex_t audioMutex;
    pthread_cond_t videoCond;
    pthread_cond_t audioCond;
    int videoQueueMaxSize = 50;      // 视频队列最大长度
    int audioQueueMaxSize = 100;      // 音频队列最大长度
    pthread_cond_t videoCondNotFull; // 视频队列未满条件
    pthread_cond_t audioCondNotFull; // 音频队列未满条件
    double audioClock = 0.00;
    double videoClock = 0.00;
    pthread_mutex_t audioClockMutex;
    bool abortPlayback = false;
    pthread_t videoThread;
    pthread_t audioThread;
    double frameRate; // 新增视频帧率
    int64_t frameDuration; // 帧持续时间(微秒)
    float frameAdjustFactor=1.0;
    void calculateFrameRate(AVStream *stream) {
        this->frameRate = av_q2d(stream->avg_frame_rate);
        this->frameDuration = (frameRate > 0) ?
                              static_cast<int64_t>(1000000 / frameRate) : 40000; // 默认25fps
    }

    ~DecodeContext() {
        cleanup();
    }

    void init() {
        pthread_mutex_init(&videoMutex, nullptr);
        pthread_mutex_init(&audioMutex, nullptr);
        pthread_cond_init(&videoCond, nullptr);
        pthread_cond_init(&audioCond, nullptr);
        pthread_mutex_init(&audioClockMutex, nullptr);
        pthread_cond_init(&videoCondNotFull, nullptr);
        pthread_cond_init(&audioCondNotFull, nullptr);
        abortPlayback = false;
        audioPlayer = new AudioPlayer();
        audioPlayer->startPlayback(44100, 2, oboe::AudioFormat::Float);
    }

    void cleanup() {
        LOGE("cleanup")
        if (audioPlayer) {
            audioPlayer->stopPlayback();
            delete audioPlayer;
            audioPlayer = nullptr;
        }
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
        pthread_cond_destroy(&videoCondNotFull);
        pthread_cond_destroy(&audioCondNotFull);
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

    void closeStream();
};

#ifdef __cplusplus
}
#endif
#endif //MEDIA_ENCODESTREAM2_H
