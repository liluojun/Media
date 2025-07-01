//
// Created by hjt on 2025/4/11.
//

#ifndef MEDIA_ENCODENAKEDSTREAM_H
#define MEDIA_ENCODENAKEDSTREAM_H
#ifdef __cplusplus

#include <chrono>
#include <thread>
#include <queue>
#include <pthread.h>
#include "AudioPlayer.h"
#include "EncodeStream2.h"
#include "FFmpegEncodeStream.h"

#include <assert.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <dirent.h>
#include "FrameType.h"
#include <vector>
#include <cstring>
#include <arpa/inet.h>

#ifdef __ANDROID__

#include <android/native_window.h>
#include <media/NdkImageReader.h>

struct OffscreenSurface {
    AImageReader *image_reader = nullptr;
    ANativeWindow *surface = nullptr;
};
#endif
using namespace std;
extern "C" {
#include "FrameCallback.h"
#include "SurfaceHolder.h"
#include "../../common/include/native_log.h"
#include "../ffmpeg/include/libavformat/avformat.h"
#include "../ffmpeg/include/libavcodec/avcodec.h"
#include "../ffmpeg/include/libavcodec/jni.h"
#include "../ffmpeg/include/libavcodec/mediacodec.h"
#include "../ffmpeg/include/libswresample/swresample.h"
#include "../ffmpeg/include/libavutil/error.h"
#include "../ffmpeg/include/libavutil/opt.h"
#include "../ffmpeg/include/libavutil/pixdesc.h"
#include "../ffmpeg/include/libswscale/swscale.h"
#include "AVSyncClock.h"
#include "sonic.h"
#include <vector>
#include <cstdint>
#include <algorithm>
#endif
struct CodecExtraData {
    std::vector<uint8_t> vps;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
};

typedef struct NakedFrameData {
    uint8_t *data;
    int size;
    long pts;
    int frameNo;
    AVCodecID codecId;
    int frametype;
    /********video独有信息**************/
    int width;
    int height;
    /********video独有信息**************/
    /********audio独有信息**************/
    int channels;
    int sampleRate;
    /********audio独有信息**************/
} NakedFrameData;
typedef struct InitContext {
    VideoDecodeContext *videoDecodeCtx = nullptr;
    AudioDecodeContext *audioDecodeCtx = nullptr;
    AudioPlayer *audioPlayer = nullptr;
    char *filePath = nullptr;
    bool abortRequest = false;
    std::queue<NakedFrameData *> videoReadDecode;
    std::queue<NakedFrameData *> audioReadDecode;
    int MAX_VIDEO_PACKET = 50;
    int MAX_AUDIO_PACKET = 200;
    pthread_mutex_t readVideoMutex;
    pthread_cond_t readVideoCond;
    pthread_mutex_t readAudioMutex;
    pthread_cond_t readAudioCond;
    int64_t videoClock = 0;
    int64_t audioClock = 0;
    bool isSoftOrHardDecod = true;
    std::shared_ptr<AVSyncClock> syncClock;
    SurfaceHolder *surfaceHolder = nullptr;


    void audioInitFailClean() {
        pthread_mutex_lock(&readAudioMutex);
        while (!audioReadDecode.empty()) {
            NakedFrameData *pkt = audioReadDecode.front();
            delete pkt;
            audioReadDecode.pop();
        }
        pthread_mutex_unlock(&readAudioMutex);
    }

    void videoInitFailClean() {
        pthread_mutex_lock(&readVideoMutex);
        while (!videoReadDecode.empty()) {
            NakedFrameData *pkt = videoReadDecode.front();
            delete pkt;
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
    }

    ~InitContext() {
        if (videoDecodeCtx) {
            delete videoDecodeCtx;
        }
        if (audioDecodeCtx) {
            delete audioDecodeCtx;
        }

    }
} InitContext;
class EncodeNakedStream {
private:
    InitContext *decodeCtx = nullptr;
    FrameCallback *frameCallback = nullptr;
    std::thread workerThread;

public:
    EncodeNakedStream();

    ~EncodeNakedStream();

    bool openStream(const char *path, jobject surface);

    void addFrame(uint8_t *frame, int type);

    void setFrameCallback(FrameCallback *callback);

    bool playbackSpeed(double speed);

    void closeStream();

};

#ifdef __cplusplus
}
#endif
#endif //MEDIA_ENCODENAKEDSTREAM_H
