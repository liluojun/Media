//
// Created by hjt on 2025/4/8.
//

#include "../include/EncodeStream2.h"

void videoThread(ReadContext *ctx) {
    while (ctx->videoStreamIndex == -1 && !ctx->videoDecodeCtx->abortRequest) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    AVCodec *videoCodec = avcodec_find_decoder(
            ctx->formatCtx->streams[ctx->videoStreamIndex]->codecpar->codec_id);
    if (!videoCodec) {
        LOGE("Video codec not found");
        ctx->videoInitFailClean();
        return;
    }
    if (!(ctx->videoDecodeCtx->videoCodecCtx = avcodec_alloc_context3(videoCodec))) {
        LOGE("Failed to allocate video codec context");
        ctx->videoInitFailClean();
        return;
    }
    if (avcodec_parameters_to_context(ctx->videoDecodeCtx->videoCodecCtx,
                                      ctx->formatCtx->streams[ctx->videoStreamIndex]->codecpar) <
        0) {

        LOGE("Failed to copy codec parameters")
        ctx->videoInitFailClean();
        return;
    }
    AVStream *videoStream = ctx->formatCtx->streams[ctx->videoStreamIndex];
    ctx->videoDecodeCtx->calculateFrameRate(videoStream);
    LOGI("Video frame rate: %.2f fps", ctx->videoDecodeCtx->frameRate);
    if (avcodec_open2(ctx->videoDecodeCtx->videoCodecCtx, videoCodec, nullptr) < 0) {
        LOGE("Failed to open video codec");
        ctx->videoInitFailClean();
        return;
    }
    while (!ctx->videoDecodeCtx->abortRequest) {
        pthread_mutex_lock(&ctx->readVideoMutex);
        while (ctx->videoReadDecode.empty() && !ctx->videoDecodeCtx->abortRequest) {
            pthread_cond_wait(&ctx->readVideoCond, &ctx->readVideoMutex);
        }
        AVPacket *packet = ctx->videoReadDecode.front();
        ctx->videoReadDecode.pop();
        pthread_mutex_unlock(&ctx->readVideoMutex);
        int ret = avcodec_send_packet(ctx->videoDecodeCtx->videoCodecCtx, packet);
        av_packet_free(&packet);
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

void videoRenderThread(ReadContext *ctx) {
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point lastRenderTime = Clock::now();
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
        double pts = frame->pts * av_q2d(ctx->formatCtx->streams[ctx->videoStreamIndex]->time_base);
        ctx->videoClock = pts;


        // 计算动态帧间隔
        double adjustedInterval = (ctx->videoDecodeCtx->frameDuration / 1000000.0) *
                                  ctx->videoDecodeCtx->frameAdjustFactor;
        // 等待逻辑（带动态调整）
        auto now = Clock::now();
        double elapsed = std::chrono::duration<double>(now - lastRenderTime).count();
        double remainingTime = adjustedInterval - elapsed;
        if (remainingTime > 0) {
            // 精确等待剩余时间（缩短等待）
            std::this_thread::sleep_for(std::chrono::duration<double>(remainingTime));
        } else {
            // 连续超时触发追赶
            static int dropCounter = 0;
            if (++dropCounter >= 2) {
                av_frame_free(&frame);
                dropCounter = 0;
                continue;
            }
        }
        // 执行渲染
        if (ctx->videoDecodeCtx->frameCallback) {
            auto renderStart = Clock::now();
            ctx->videoDecodeCtx->frameCallback->onFrameEncoded(frame);
            auto renderDuration = Clock::now() - renderStart;

            // 补偿渲染耗时
            lastRenderTime = Clock::now() + renderDuration;
        } else {
            lastRenderTime = Clock::now();
        }

        av_frame_free(&frame);
    }
    return;
}

void audioThread(ReadContext *ctx) {
    while (ctx->audioStreamIndex == -1 && !ctx->audioDecodeCtx->abortRequest) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    SwrContext *swr_ctx = nullptr;//音频重采样
    AVCodec *audioCodec = avcodec_find_decoder(
            ctx->formatCtx->streams[ctx->audioStreamIndex]->codecpar->codec_id);
    if (!audioCodec) {
        LOGE("audioCodec codec not found");
        ctx->audioInitFailClean();
        return;
    }
    if (!(ctx->audioDecodeCtx->audioDecodeCtx = avcodec_alloc_context3(audioCodec))) {
        LOGE("Failed to allocate video codec context");
        ctx->audioInitFailClean();
        return;
    }
    if (avcodec_parameters_to_context(ctx->audioDecodeCtx->audioDecodeCtx,
                                      ctx->formatCtx->streams[ctx->audioStreamIndex]->codecpar) <
        0) {
        LOGE("Failed to copy codec parameters")
        ctx->audioInitFailClean();
        return;
    }

    if (avcodec_open2(ctx->audioDecodeCtx->audioDecodeCtx, audioCodec, nullptr) < 0) {
        LOGE("Failed to open audioCodec codec");
        ctx->audioInitFailClean();
        return;
    }
    swr_ctx = swr_alloc_set_opts(
            nullptr,
            av_get_default_channel_layout(ctx->audioDecodeCtx->audioDecodeCtx->channels),
            AV_SAMPLE_FMT_FLT, // 目标格式：S16交错
            ctx->audioDecodeCtx->audioDecodeCtx->sample_rate,
            av_get_default_channel_layout(ctx->audioDecodeCtx->audioDecodeCtx->channels),
            (AVSampleFormat) ctx->audioDecodeCtx->audioDecodeCtx->sample_fmt,
            ctx->audioDecodeCtx->audioDecodeCtx->sample_rate,
            0, nullptr
    );
    if (swr_init(swr_ctx) < 0) {
        LOGE("Failed to initialize resampler");
        ctx->audioInitFailClean();
        return;
    }
    if (ctx->audioPlayer) {
        ctx->audioPlayer->startPlayback(ctx->audioDecodeCtx->audioDecodeCtx->sample_rate,
                                        ctx->audioDecodeCtx->audioDecodeCtx->channels,
                                        oboe::AudioFormat::Float);
        ctx->audioPlayInit = true;
    }
    while (!ctx->audioDecodeCtx->abortRequest) {
        pthread_mutex_lock(&ctx->readAudioMutex);
        while (ctx->audioReadDecode.empty() && !ctx->audioDecodeCtx->abortRequest) {
            pthread_cond_wait(&ctx->readAudioCond, &ctx->readAudioMutex);
        }
        AVPacket *packet = ctx->audioReadDecode.front();
        ctx->audioReadDecode.pop();
        pthread_mutex_unlock(&ctx->readAudioMutex);
        int ret = avcodec_send_packet(ctx->audioDecodeCtx->audioDecodeCtx, packet);
        av_packet_free(&packet);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            LOGE("Error sending packet: %s", av_err2str(ret));
            continue;
        }
        AVFrame *frame = av_frame_alloc();
        while (avcodec_receive_frame(ctx->audioDecodeCtx->audioDecodeCtx, frame) == 0) {
            int64_t delay = swr_get_delay(swr_ctx, frame->sample_rate);
            // 预分配输出缓冲区
            int max_out_samples = av_rescale_rnd(
                    frame->nb_samples + delay,
                    ctx->audioDecodeCtx->audioDecodeCtx->sample_rate,
                    ctx->audioDecodeCtx->audioDecodeCtx->sample_rate,
                    AV_ROUND_UP
            );
            uint8_t *output_buffer = nullptr;
            int out_linesize;
            av_samples_alloc(&output_buffer, &out_linesize,
                             ctx->audioDecodeCtx->audioDecodeCtx->channels,
                             max_out_samples,
                             AV_SAMPLE_FMT_FLT, 0);

            int out_samples = swr_convert(
                    swr_ctx,
                    &output_buffer,
                    frame->nb_samples,
                    (const uint8_t **) frame->data,
                    frame->nb_samples
            );
            if (out_samples > 0) {
                int bufSize = av_samples_get_buffer_size(
                        nullptr, ctx->audioDecodeCtx->audioDecodeCtx->channels,
                        out_samples, AV_SAMPLE_FMT_FLT, 0
                );
                AudioData audioData;
                audioData.data = (uint8_t *) av_malloc(bufSize);
                memcpy(audioData.data, output_buffer, bufSize);
                audioData.size = bufSize;
                audioData.pts = frame->pts *
                                av_q2d(ctx->formatCtx->streams[ctx->audioStreamIndex]->time_base);
                pthread_mutex_lock(&ctx->audioDecodeCtx->audioDecodeMutex);
                // 等待直到队列有空位
                while (ctx->audioDecodeCtx->audioDecodeQueue.size() >=
                       ctx->audioDecodeCtx->MAX_AUDIO_FRAME &&
                       !ctx->abortRequest) {
                    pthread_cond_wait(&ctx->audioDecodeCtx->audioDecodeFullCond,
                                      &ctx->audioDecodeCtx->audioDecodeMutex);
                }
                ctx->audioDecodeCtx->audioDecodeQueue.push(audioData);
                pthread_cond_signal(&ctx->audioDecodeCtx->audioDecodeEmptyCond);
                pthread_mutex_unlock(&ctx->audioDecodeCtx->audioDecodeMutex);
            }
        }
    }

}

void audioRenderThread(ReadContext *ctx) {
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
        if (ctx->audioPlayer && ctx->audioPlayInit) {
            ctx->audioPlayer->writeData(audioData.data, audioData.size);
        }
        if (audioData.data) {
            av_free(audioData.data);
        }
    }
    return;
}

void readThread(ReadContext *ctx) {
    // 打开输入文件
    if (avformat_open_input(&ctx->formatCtx, ctx->filePath, nullptr, nullptr) != 0) {
        LOGE("Failed to open input file");
        delete ctx;
        return;
    }

    if (avformat_find_stream_info(ctx->formatCtx, nullptr) < 0) {
        LOGE("Failed to find stream information");
        delete ctx;
        return;
    }

    // 查找视频和音频流
    for (unsigned int i = 0; i < ctx->formatCtx->nb_streams; i++) {
        AVCodecParameters *codecParams = ctx->formatCtx->streams[i]->codecpar;
        if (codecParams->codec_type == AVMEDIA_TYPE_VIDEO && ctx->videoStreamIndex == -1) {
            ctx->videoStreamIndex = i;
        } else if (codecParams->codec_type == AVMEDIA_TYPE_AUDIO && ctx->audioStreamIndex == -1) {
            ctx->audioStreamIndex = i;
        }
    }

    while (!ctx->abortRequest) {
        AVPacket *p = av_packet_alloc();
        if (av_read_frame(ctx->formatCtx, p) < 0) {
            av_packet_free(&p);
            break;
        }
        if (p->stream_index == ctx->videoStreamIndex && !ctx->videoInitFail) {
            pthread_mutex_lock(&ctx->readVideoMutex);

            ctx->videoReadDecode.push(p);
            pthread_cond_signal(&ctx->readVideoCond);
            // LOGE("3  ctx->videoReadDecode size=%d", ctx->videoReadDecode.size())
            pthread_mutex_unlock(&ctx->readVideoMutex);

        } else if (p->stream_index == ctx->audioStreamIndex && !ctx->audioInitFail) {
            pthread_mutex_lock(&ctx->readAudioMutex);
            ctx->audioReadDecode.push(p);
            pthread_cond_signal(&ctx->readAudioCond);
            //LOGE("4  ctx->audioReadDecode size=%d", ctx->audioReadDecode.size())
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
    LOGE("END")
    return;
}

bool EncodeStream2::openStream(const char *filePath) {
    try {
        closeStream();
        decodeCtx = new ReadContext();
        decodeCtx->init();
        decodeCtx->filePath = av_strdup(filePath);
        decodeCtx->videoDecodeCtx = new VideoDecodeContext();
        decodeCtx->videoDecodeCtx->init();
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

void EncodeStream2::closeStream() {
    if (decodeCtx) {
        decodeCtx->abortRequest = true; // 添加终止标志
        if (workerThread.joinable()) {
            workerThread.join(); // 替换 pthread_join
        }
        delete decodeCtx;
        decodeCtx = nullptr;

    }
}

void EncodeStream2::setFrameCallback(FrameCallback *callback) {
    frameCallback = callback;
    if (decodeCtx && decodeCtx->videoDecodeCtx) {
        decodeCtx->videoDecodeCtx->frameCallback = callback;
    }
}

EncodeStream2::EncodeStream2() {}

EncodeStream2::~EncodeStream2() {
    closeStream();
}
