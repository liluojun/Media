//
// Created by hjt on 2025/4/11.
//

#include "../include/EncodeNakedStream.h"

bool parseHeader(const std::vector<uint8_t> &header, NakedFrameData *data) {
    try {
        data->frametype = *reinterpret_cast<const int64_t *>(&header[4]);

        data->pts = *reinterpret_cast<const int64_t *>(&header[12]);
        data->size = *reinterpret_cast<const int *>(&header[20]);
        data->width = *reinterpret_cast<const int16_t *>(&header[24]);
        data->height = *reinterpret_cast<const int16_t *>(&header[26]);
        data->data = new uint8_t[data->size];
        /*LOGE("data->frametype=%d    data->size=%d    data->pts=%d", data->frametype, data->size,
             data->pts)*/
        int codeId = *reinterpret_cast<const int8_t *>(&header[28]);
        switch (codeId) {
            case 0:
                data->codecId = AV_CODEC_ID_H264;
                break;
            case 0x10:
                data->codecId = AV_CODEC_ID_H265;

                break;
            default:
                data->codecId = AV_CODEC_ID_NONE;
                break;
        }
        return true;
    } catch (const std::exception &e) {
        LOGE("解析头失败");
        return false;
    }


}

int initVideoAVCodec(InitContext *ctx, AVCodec *videoCodec, NakedFrameData *data,
                     AVCodecID *lastAVCodecID) {
    if (videoCodec != nullptr || (*lastAVCodecID == AV_CODEC_ID_NONE) ||
        (*lastAVCodecID != AV_CODEC_ID_NONE && *lastAVCodecID != data->codecId) ||
        (ctx->videoDecodeCtx->videoCodecCtx == nullptr) ||
        (ctx->videoDecodeCtx->videoCodecCtx->width != data->width) ||
        (ctx->videoDecodeCtx->videoCodecCtx->height != data->height)) {
        if (ctx->videoDecodeCtx->videoCodecCtx != nullptr) {
            avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
        }
        videoCodec = avcodec_find_decoder(data->codecId);
        *lastAVCodecID = data->codecId;
        if (!videoCodec) {
            LOGE("Video codec not found");
            ctx->videoInitFailClean();
            return -2;
        }
        if (!(ctx->videoDecodeCtx->videoCodecCtx = avcodec_alloc_context3(videoCodec))) {
            LOGE("Failed to allocate video codec context");
            ctx->videoInitFailClean();
            return -3;
        }
        ctx->videoDecodeCtx->videoCodecCtx->width = data->width;
        ctx->videoDecodeCtx->videoCodecCtx->height = data->height;
        ctx->videoDecodeCtx->videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        ctx->videoDecodeCtx->user_time_base = (AVRational) {1, 25};
        ctx->videoDecodeCtx->frame_count = 0;
        if (avcodec_open2(ctx->videoDecodeCtx->videoCodecCtx, videoCodec, NULL) < 0) {
            avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
            ctx->videoInitFailClean();
            return -4;
        }
    }
    return 0;
}

void videoThread(InitContext *ctx) {
    AVCodec *videoCodec = nullptr;
    AVCodecID lastAVCodecID = AV_CODEC_ID_NONE;
    ctx->videoDecodeCtx->calculateFrameRateNaked();
    while (!ctx->videoDecodeCtx->abortRequest) {
        pthread_mutex_lock(&ctx->readVideoMutex);
        while (ctx->videoReadDecode.empty() && !ctx->videoDecodeCtx->abortRequest) {
            pthread_cond_wait(&ctx->readVideoCond, &ctx->readVideoMutex);
        }
        NakedFrameData *packet = ctx->videoReadDecode.front();
        ctx->videoReadDecode.pop();
        pthread_mutex_unlock(&ctx->readVideoMutex);
        int ret = initVideoAVCodec(ctx, videoCodec, packet, &lastAVCodecID);
        if (ret < 0) {
            LOGE("initVideoAVCodec error=%d", ret)
            delete packet;
            continue;
        }
        AVPacket *pkt = av_packet_alloc();
        pkt->data = packet->data;
        pkt->size = packet->size;
        ret = avcodec_send_packet(ctx->videoDecodeCtx->videoCodecCtx, pkt);
        av_packet_free(&pkt);
        delete packet;
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            LOGE("Error sending packet: %s", av_err2str(ret));
            continue;
        }
        AVFrame *frame = av_frame_alloc();
        while (avcodec_receive_frame(ctx->videoDecodeCtx->videoCodecCtx, frame) == 0 &&
               !ctx->videoDecodeCtx->abortRequest) {
            AVFrame *frameCopy = av_frame_clone(frame);
            pthread_mutex_lock(&ctx->videoDecodeCtx->viedoDecodeMutex);
            while (ctx->videoDecodeCtx->videoDecodeQueue.size() >=
                   ctx->videoDecodeCtx->MAX_VIDEO_FRAME &&
                   !ctx->videoDecodeCtx->abortRequest) {
                pthread_cond_wait(&ctx->videoDecodeCtx->viedoDecodeFullCond,
                                  &ctx->videoDecodeCtx->viedoDecodeMutex);
            }
            ctx->videoDecodeCtx->videoDecodeQueue.push(frameCopy);
            pthread_cond_signal(&ctx->videoDecodeCtx->viedoDecodeEmptyCond);
            pthread_mutex_unlock(&ctx->videoDecodeCtx->viedoDecodeMutex);
        }
    }
}

void videoRenderThread(InitContext *ctx) {
    while (!ctx->videoDecodeCtx->renderRequest) {
        pthread_mutex_lock(&ctx->videoDecodeCtx->viedoDecodeMutex);
        while (ctx->videoDecodeCtx->videoDecodeQueue.empty() &&
               !ctx->videoDecodeCtx->renderRequest) {
            pthread_cond_wait(&ctx->videoDecodeCtx->viedoDecodeEmptyCond,
                              &ctx->videoDecodeCtx->viedoDecodeMutex);
        }
        if (ctx->videoDecodeCtx->renderRequest) {
            pthread_mutex_unlock(&ctx->videoDecodeCtx->viedoDecodeMutex);
            break;
        }

        AVFrame *frame = ctx->videoDecodeCtx->videoDecodeQueue.front();
        ctx->videoDecodeCtx->videoDecodeQueue.pop();

        // 通知解码线程队列有空位
        pthread_cond_signal(&ctx->videoDecodeCtx->viedoDecodeFullCond);
        pthread_mutex_unlock(&ctx->videoDecodeCtx->viedoDecodeMutex);
        double pts;
        if (frame->pts != AV_NOPTS_VALUE) {
            // 使用解码器提供的PTS
            pts = frame->pts * av_q2d(ctx->videoDecodeCtx->user_time_base);
        } else {
            // 人工生成线性PTS
            pts = ctx->videoDecodeCtx->frame_count * av_q2d(ctx->videoDecodeCtx->user_time_base);
            frame->pts = ctx->videoDecodeCtx->frame_count;
            ctx->videoDecodeCtx->frame_count++;
        }
        if (!ctx->videoDecodeCtx->syncClockInitialized) {
            ctx->syncClock->init((int64_t) (pts * AV_TIME_BASE));
            ctx->videoDecodeCtx->syncClockInitialized = true;
            LOGE("首次渲染视频帧，系统时间基准建立");
        }

        int64_t framePtsUs = (int64_t) (pts * AV_TIME_BASE);
        ctx->videoClock = framePtsUs;
        if (!ctx->syncClock->shouldRenderVideo(framePtsUs)) {
            std::this_thread::sleep_for(std::chrono::microseconds(
                    (framePtsUs - ctx->syncClock->getVideoPlayTimeUs())) /
                                        ctx->syncClock->getPlaybackSpeed());
        }

        // 执行渲染
        if (ctx->videoDecodeCtx->frameCallback) {
            ctx->videoDecodeCtx->frameCallback->onFrameEncoded(frame);
        }

        av_frame_free(&frame);
    }
    return;
}

int initAudioAVCodec(InitContext *ctx, AVCodec *audioCodec, NakedFrameData *data) {
    if (audioCodec != nullptr ||
        (ctx->audioDecodeCtx->audioDecodeCtx == nullptr)) {
        if (ctx->audioDecodeCtx->audioDecodeCtx != nullptr) {
            avcodec_free_context(&ctx->audioDecodeCtx->audioDecodeCtx);
        }
        audioCodec = avcodec_find_decoder(AV_CODEC_ID_PCM_MULAW);

        if (!audioCodec) {
            LOGE("Video codec not found");
            ctx->audioInitFailClean();
            return -2;
        }
        if (!(ctx->audioDecodeCtx->audioDecodeCtx = avcodec_alloc_context3(audioCodec))) {
            LOGE("Failed to allocate video codec context");
            ctx->audioInitFailClean();
            return -3;
        }
        ctx->audioDecodeCtx->audioDecodeCtx->channels = 1;
        ctx->audioDecodeCtx->audioDecodeCtx->sample_rate = 8000;
        ctx->audioDecodeCtx->audioDecodeCtx->channel_layout = AV_CH_LAYOUT_MONO;
        ctx->audioDecodeCtx->audioDecodeCtx->sample_fmt = AV_SAMPLE_FMT_S16;
        ctx->audioPlayer->startPlayback(ctx->audioDecodeCtx->audioDecodeCtx->sample_rate,
                                        ctx->audioDecodeCtx->audioDecodeCtx->channels,
                                        oboe::AudioFormat::I16);
        if (avcodec_open2(ctx->audioDecodeCtx->audioDecodeCtx, audioCodec, NULL) < 0) {
            avcodec_free_context(&ctx->audioDecodeCtx->audioDecodeCtx);
            ctx->audioInitFailClean();
            return -4;
        }
    }
    return 0;
}

void audioThread(InitContext *ctx) {
    AVCodec *audioCodec = nullptr;
    while (!ctx->audioDecodeCtx->abortRequest) {
        pthread_mutex_lock(&ctx->readAudioMutex);
        while (ctx->audioReadDecode.empty() && !ctx->audioDecodeCtx->abortRequest) {
            pthread_cond_wait(&ctx->readAudioCond, &ctx->readAudioMutex);
        }
        NakedFrameData *packet = ctx->audioReadDecode.front();
        ctx->audioReadDecode.pop();
        pthread_mutex_unlock(&ctx->readAudioMutex);
        int ret = initAudioAVCodec(ctx, audioCodec, packet);
        if (ret < 0) {
            LOGE("initVideoAVCodec error=%d", ret)
            delete packet;
            continue;
        }
        AVPacket *pkt = av_packet_alloc();
        pkt->data = packet->data;
        pkt->size = packet->size;
        ret = avcodec_send_packet(ctx->audioDecodeCtx->audioDecodeCtx, pkt);
        av_packet_free(&pkt);
        delete packet;
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            LOGE("Error sending packet: %s", av_err2str(ret));
            continue;
        }
        AVFrame *frame = av_frame_alloc();
        while (avcodec_receive_frame(ctx->audioDecodeCtx->audioDecodeCtx, frame) == 0 &&
               !ctx->audioDecodeCtx->abortRequest) {
            int data_size =
                    frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat) frame->format) *
                    frame->channels;
            AudioData nakedFrameData;
            uint8_t *pcm_buffer = (uint8_t *) malloc(data_size);
            memcpy(pcm_buffer, frame->data[0], data_size);
            nakedFrameData.data = pcm_buffer;
            nakedFrameData.size = data_size;
            int64_t pts_increment_per_frame =
                    frame->nb_samples * AV_TIME_BASE / ctx->audioDecodeCtx->sample_rate;
            nakedFrameData.pts = ctx->audioDecodeCtx->pts;
            ctx->audioDecodeCtx->pts += pts_increment_per_frame;
            pthread_mutex_lock(&ctx->audioDecodeCtx->audioDecodeMutex);
            while (ctx->audioDecodeCtx->audioDecodeQueue.size() >=
                   ctx->audioDecodeCtx->MAX_AUDIO_FRAME &&
                   !ctx->audioDecodeCtx->abortRequest) {
                pthread_cond_wait(&ctx->audioDecodeCtx->audioDecodeFullCond,
                                  &ctx->audioDecodeCtx->audioDecodeMutex);
            }
            ctx->audioDecodeCtx->audioDecodeQueue.push(nakedFrameData);
            pthread_cond_signal(&ctx->audioDecodeCtx->audioDecodeEmptyCond);
            pthread_mutex_unlock(&ctx->audioDecodeCtx->audioDecodeMutex);
        }
    }
}

void audioRenderThread(InitContext *ctx) {
    while (!ctx->audioDecodeCtx->playRequest) {
        pthread_mutex_lock(&ctx->audioDecodeCtx->audioDecodeMutex);
        while (ctx->audioDecodeCtx->audioDecodeQueue.empty() && !ctx->audioDecodeCtx->playRequest) {
            pthread_cond_wait(&ctx->audioDecodeCtx->audioDecodeEmptyCond,
                              &ctx->audioDecodeCtx->audioDecodeMutex);
        }
        AudioData audioData = ctx->audioDecodeCtx->audioDecodeQueue.front();
        ctx->audioDecodeCtx->audioDecodeQueue.pop();
        // 通知解码线程队列有空位
        pthread_cond_signal(&ctx->audioDecodeCtx->audioDecodeFullCond);
        pthread_mutex_unlock(&ctx->audioDecodeCtx->audioDecodeMutex);
        ctx->audioClock = audioData.pts;// +
        int64_t deltaUs = ctx->syncClock->syncAudio(audioData.pts);
        LOGE("pts* videoClock=%d   audioClock=%d deltaUs=%d", ctx->videoClock, ctx->audioClock,
             deltaUs)
        if (deltaUs > ctx->syncClock->getSleepThresholdUs()) {
            // 提前太多，sleep
            std::this_thread::sleep_for(std::chrono::microseconds(
                    static_cast<int64_t>(deltaUs / ctx->syncClock->getPlaybackSpeed())));
        } else if (deltaUs < ctx->syncClock->getDropThresholdUs()) {
            // 落后太多，丢弃
            LOGW("drop audio");
            av_free(audioData.data);
            continue;
        }
        if (ctx->audioPlayer) {
            ctx->audioPlayer->writeData(audioData.data, audioData.size);
        }
        if (audioData.data) {
            av_free(audioData.data);
        }
    }
    return;
}

void readThread(InitContext *ctx) {
    while (!ctx->abortRequest) {
        if (ctx->filePath) {
            ifstream file(ctx->filePath, ios::binary);
            if (!file) {
                LOGE("file error %s", ctx->filePath);
                return;
            }

            while (file && !ctx->abortRequest) {
                vector<uint8_t> header(36);
                if (!file.read(reinterpret_cast<char *>(header.data()), 36)) {
                    if (file.gcount() < 36) {
                        LOGE("文件头不完整%s  %d", ctx->filePath, file.gcount());
                        ctx->abortRequest = true;
                        break;
                    }
                }
                NakedFrameData *data = new NakedFrameData();
                if (!parseHeader(header, data)) {
                    LOGE("文件头解析异常")
                    delete data;
                    continue;
                }

                if (!file.read(reinterpret_cast<char *>(data->data), data->size)) {
                    delete data;
                    continue;
                }
                if (data->frametype == PktIFrames
                    || data->frametype == PktPFrames) {
                    pthread_mutex_lock(&ctx->readVideoMutex);
                    ctx->videoReadDecode.push(data);
                    pthread_cond_signal(&ctx->readVideoCond);
                    pthread_mutex_unlock(&ctx->readVideoMutex);
                } else if (data->frametype == PktAudioFrames) {
                    pthread_mutex_lock(&ctx->readAudioMutex);
                    ctx->audioReadDecode.push(data);
                    pthread_cond_signal(&ctx->readAudioCond);
                    pthread_mutex_unlock(&ctx->readAudioMutex);
                }
                // 检查队列是否需要等待
                bool needWait;
                do {
                    pthread_mutex_lock(&ctx->readVideoMutex);
                    bool videoFull = ctx->videoReadDecode.size() > ctx->MAX_VIDEO_PACKET;
                    bool audioFull = ctx->audioReadDecode.size() > ctx->MAX_AUDIO_PACKET;
                    needWait = videoFull || audioFull;
                    pthread_mutex_unlock(&ctx->readVideoMutex);
                    if (needWait && !ctx->abortRequest) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 更短的时间以快速响应终止
                    }
                } while (needWait && !ctx->abortRequest); // 任一队列满且未终止时等待
            }
        }
    }
    LOGE("END")
    return;
}

bool EncodeNakedStream::openStream(const char *filePath) {
    try {
        closeStream();
        decodeCtx = new InitContext();
        decodeCtx->init();
        decodeCtx->filePath = av_strdup(filePath);
        decodeCtx->videoDecodeCtx = new VideoDecodeContext();
        decodeCtx->videoDecodeCtx->init();
        decodeCtx->syncClock = std::make_shared<AVSyncClock>();
        decodeCtx->audioDecodeCtx = new AudioDecodeContext();
        decodeCtx->audioDecodeCtx->init();
        decodeCtx->videoDecodeCtx->frameCallback = frameCallback;
        decodeCtx->videoDecodeCtx->videoDecodeThread = std::thread(videoThread, decodeCtx);
        decodeCtx->videoDecodeCtx->videoRendderThread = std::thread(videoRenderThread, decodeCtx);
        decodeCtx->audioDecodeCtx->audioDecodeThread = std::thread(audioThread, decodeCtx);
        decodeCtx->audioDecodeCtx->audioRendderThread = std::thread(audioRenderThread, decodeCtx);
        workerThread = std::thread(readThread, decodeCtx);
    } catch (const std::system_error &e) {
        delete decodeCtx;
        decodeCtx = nullptr;
        return false;
    }
    return true;
}

void EncodeNakedStream::closeStream() {
    if (decodeCtx) {
        decodeCtx->abortRequest = true; // 添加终止标志
        if (workerThread.joinable()) {
            workerThread.join(); // 替换 pthread_join
        }
        delete decodeCtx;
        decodeCtx = nullptr;

    }
}

void EncodeNakedStream::setFrameCallback(FrameCallback *callback) {
    frameCallback = callback;
    if (decodeCtx && decodeCtx->videoDecodeCtx) {
        decodeCtx->videoDecodeCtx->frameCallback = callback;
    }
}

bool EncodeNakedStream::playbackSpeed(double speed) {
    if (decodeCtx && decodeCtx->syncClock) {
        decodeCtx->syncClock->setPlaybackSpeed(speed);
        return true;
    } else {
        return false;
    }
}

EncodeNakedStream::EncodeNakedStream() {}

EncodeNakedStream::~EncodeNakedStream() {
    closeStream();
}