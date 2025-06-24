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
#include "FFmpegEncodeStream.h"

extern "C" {
#include "FrameCallback.h"
#include "../../common/include/native_log.h"
#include "../ffmpeg/include/libavformat/avformat.h"
#include "../ffmpeg/include/libavcodec/avcodec.h"
#include "../ffmpeg/include/libswresample/swresample.h"

#endif

typedef struct ViedoDecodeContext {
    AVCodecContext *videoCodecCtx = nullptr;
    bool abortRequest = false;
    bool renderRequest = false;
    std::queue<AVFrame *> videoDecodeQueue;
    int MAX_VIDEO_FRAME = 50;
    pthread_mutex_t viedoDecodeMutex;
    pthread_cond_t viedoDecodeFullCond;
    pthread_cond_t viedoDecodeEmptyCond;
    std::thread videoDecodeThread;
    std::thread videoRendderThread;
    double frameRate; // 新增视频帧率
    int64_t frameDuration; // 帧持续时间(微秒)
    float frameAdjustFactor = 1.0;
    FrameCallback *frameCallback = nullptr;
    AVRational user_time_base;
    int frame_count;
    void calculateFrameRate(AVStream *stream) {
        this->frameRate = av_q2d(stream->avg_frame_rate);
        this->frameDuration = (frameRate > 0) ?
                              static_cast<int64_t>(1000000 / frameRate) : 40000; // 默认25fps
    }
 void calculateFrameRateNaked() {
        this->frameRate = 25;
        this->frameDuration = 40000; // 默认25fps
    }

    void init() {
        pthread_mutex_init(&viedoDecodeMutex, nullptr);
        pthread_cond_init(&viedoDecodeFullCond, nullptr);
        pthread_cond_init(&viedoDecodeEmptyCond, nullptr);
    };

    ~ViedoDecodeContext() {
        abortRequest = true;
        renderRequest = true;
        pthread_cond_signal(&viedoDecodeFullCond);
        pthread_cond_signal(&viedoDecodeEmptyCond);
        if (videoCodecCtx) {
            avcodec_free_context(&videoCodecCtx);
        }
        if (videoDecodeThread.joinable()) {
            videoDecodeThread.join();
        }
        if (videoRendderThread.joinable()) {
            videoRendderThread.join();
        }
    }
} VideoDecodeContext;
typedef struct AudioDecodeContext {
    AVCodecContext *audioDecodeCtx = nullptr;
    bool abortRequest = false;
    bool playRequest = false;
    std::queue<AudioData> audioDecodeQueue;
    int MAX_AUDIO_FRAME = 50;
    pthread_mutex_t audioDecodeMutex;
    pthread_cond_t audioDecodeFullCond;
    pthread_cond_t audioDecodeEmptyCond;
    std::thread audioDecodeThread;
    std::thread audioRendderThread;
    AVRational user_time_base;
    int pts=0;
    int sample_rate=8000;
    void init() {
        pthread_mutex_init(&audioDecodeMutex, nullptr);
        pthread_cond_init(&audioDecodeFullCond, nullptr);
        pthread_cond_init(&audioDecodeEmptyCond, nullptr);
    };

    ~AudioDecodeContext() {
        abortRequest = true;
        playRequest = true;
        pthread_cond_signal(&audioDecodeFullCond);
        pthread_cond_signal(&audioDecodeEmptyCond);
        if (audioDecodeCtx) {
            avcodec_free_context(&audioDecodeCtx);
        }
        if (audioDecodeThread.joinable()) {
            audioDecodeThread.join();
        }
        if (audioRendderThread.joinable()) {
            audioRendderThread.join();
        }
    }
} AudioDecodeContext;
typedef struct ReadContext {
    VideoDecodeContext *videoDecodeCtx = nullptr;
    AudioDecodeContext *audioDecodeCtx = nullptr;
    bool videoInitFail = false;
    bool audioInitFail = false;
    AudioPlayer *audioPlayer = nullptr;
    AVFormatContext *formatCtx = nullptr;
    char *filePath = nullptr;
    bool abortRequest = false;
    bool audioPlayInit = false;
    std::queue<AVPacket *> videoReadDecode;
    std::queue<AVPacket *> audioReadDecode;
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int MAX_VIDEO_PACKET = 50;
    int MAX_AUDIO_PACKET = 200;
    pthread_mutex_t readVideoMutex;
    pthread_cond_t readVideoCond;
    pthread_mutex_t readAudioMutex;
    pthread_cond_t readAudioCond;
    int videoClock = 0;
    int audioClock = 0;

    void audioInitFailClean() {
        pthread_mutex_lock(&readAudioMutex);
        audioInitFail = true;
        while (!audioReadDecode.empty()) {
            AVPacket *pkt = audioReadDecode.front();
            av_packet_free(&pkt);  // FFmpeg 的标准释放方式
            audioReadDecode.pop();
        }
        pthread_mutex_unlock(&readAudioMutex);
    }

    void videoInitFailClean() {
        pthread_mutex_lock(&readVideoMutex);
        videoInitFail = true;
        while (!videoReadDecode.empty()) {
            AVPacket *pkt = videoReadDecode.front();
            av_packet_free(&pkt);  // FFmpeg 的标准释放方式
            videoReadDecode.pop();
        }
        pthread_mutex_unlock(&readVideoMutex);
    }

    void init() {
        pthread_mutex_init(&readVideoMutex, nullptr);
        pthread_cond_init(&readVideoCond, nullptr);
        pthread_mutex_init(&readAudioMutex, nullptr);
        pthread_cond_init(&readAudioCond, nullptr);
        audioPlayer = new AudioPlayer();
    };

    ~ReadContext() {
        if (videoDecodeCtx) {
            delete videoDecodeCtx;
        }
        if (audioDecodeCtx) {
            delete audioDecodeCtx;
        }
        if (formatCtx) {
            avformat_close_input(&formatCtx);
        }
    }
} ReadContext;

class EncodeStream2 {
private:
    ReadContext *decodeCtx = nullptr;
    FrameCallback *frameCallback = nullptr;
    std::thread workerThread;
public:

    EncodeStream2();

    ~EncodeStream2();

    bool openStream(const char *path);

    void setFrameCallback(FrameCallback *callback);

    void closeStream();
};

#ifdef __cplusplus
}
#endif
#endif //MEDIA_ENCODESTREAM2_H
