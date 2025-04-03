#include "FFmpegEncodeStream.h"
#include "Utils.cpp"
#include <chrono>
#include <ctime>

#define MAX_AUDIO_FRAME_SIZE 44100
#define usleep(useconds) (::usleep(static_cast<useconds_t>(useconds)))
// 获取当前时间戳 (毫秒级)
auto now = std::chrono::system_clock::now();
auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
std::ofstream file("storage/emulated/0/Android/data/com.git.media/files/audio.pcm",
                   std::ios::binary);

// 新增函数声明
void *videoPlayThread(void *arg);

void *audioPlayThread(void *arg);

bool containsSEI(const uint8_t *data, int size) {
    if (!data || size < 4) return false; // 新增空指针和最小长度检查
    for (int i = 0; i < size - 4; i++) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 &&
            data[i + 2] == 0x00 && data[i + 3] == 0x06) { // NAL Unit Type 6 (SEI)
            return true;
        }
    }
    return false;
}

bool extractSEIData(const uint8_t *data, int size, uint8_t **seiData, int *seiSize) {
    for (int i = 0; i < size - 4; i++) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 &&
            data[i + 2] == 0x00 && data[i + 3] == 0x06) { // SEI NAL 头
            int seiStart = i + 4;
            int seiEnd = size;

            // 计算 SEI 结束位置
            for (int j = seiStart; j < size - 3; j++) {
                if (data[j] == 0x00 && data[j + 1] == 0x00 &&
                    (data[j + 2] == 0x01 || data[j + 2] == 0x03)) { // 下一个 NAL 头
                    seiEnd = j;
                    break;
                }
            }

            *seiSize = seiEnd - seiStart;
            *seiData = (uint8_t *) malloc(*seiSize);
            memcpy(*seiData, data + seiStart, *seiSize);
            return true;
        }
    }
    return false;
}

void *decodeThread(void *arg) {
    DecodeContext *ctx = static_cast<DecodeContext *>(arg);
    AVCodec *videoCodec = nullptr, *audioCodec = nullptr;
    SwrContext *swr_ctx = nullptr;//音频重采样
    // 打开输入文件
    if (avformat_open_input(&ctx->formatCtx, ctx->filePath, nullptr, nullptr) != 0) {
        LOGE("Failed to open input file");
        delete ctx;
        return nullptr;
    }

    if (avformat_find_stream_info(ctx->formatCtx, nullptr) < 0) {
        LOGE("Failed to find stream information");
        delete ctx;
        return nullptr;
    }

    // 查找视频和音频流
    for (unsigned int i = 0; i < ctx->formatCtx->nb_streams; i++) {
        AVCodecParameters *codecParams = ctx->formatCtx->streams[i]->codecpar;
        if (codecParams->codec_type == AVMEDIA_TYPE_VIDEO && ctx->videoStreamIndex == -1) {
            ctx->videoStreamIndex = i;
            videoCodec = avcodec_find_decoder(codecParams->codec_id);
        } else if (codecParams->codec_type == AVMEDIA_TYPE_AUDIO && ctx->audioStreamIndex == -1) {
            ctx->audioStreamIndex = i;
            audioCodec = avcodec_find_decoder(codecParams->codec_id);
        }
    }

    // 初始化视频解码器
    if (ctx->videoStreamIndex != -1 && videoCodec) {
        ctx->videoCodecCtx = avcodec_alloc_context3(videoCodec);
        avcodec_parameters_to_context(ctx->videoCodecCtx,
                                      ctx->formatCtx->streams[ctx->videoStreamIndex]->codecpar);
        AVStream *videoStream = ctx->formatCtx->streams[ctx->videoStreamIndex];
        ctx->calculateFrameRate(videoStream);
        LOGI("Video frame rate: %.2f fps", ctx->frameRate);
        if (avcodec_open2(ctx->videoCodecCtx, videoCodec, nullptr) < 0) {
            LOGE("Failed to open video codec");
            delete ctx;
            return nullptr;
        }
    }

    // 初始化音频解码器
    if (ctx->audioStreamIndex != -1 && audioCodec) {
        ctx->audioCodecCtx = avcodec_alloc_context3(audioCodec);
        avcodec_parameters_to_context(ctx->audioCodecCtx,
                                      ctx->formatCtx->streams[ctx->audioStreamIndex]->codecpar);
        if (avcodec_open2(ctx->audioCodecCtx, audioCodec, nullptr) < 0) {
            LOGE("Failed to open audio codec");
            delete ctx;
            return nullptr;
        }
        swr_ctx = swr_alloc_set_opts(
                nullptr,
                av_get_default_channel_layout(ctx->audioCodecCtx->channels),
                AV_SAMPLE_FMT_FLT, // 目标格式：S16交错
                ctx->audioCodecCtx->sample_rate,
                av_get_default_channel_layout(ctx->audioCodecCtx->channels),
                (AVSampleFormat) ctx->audioCodecCtx->sample_fmt,
                ctx->audioCodecCtx->sample_rate,
                0, nullptr
        );
        if (swr_init(swr_ctx) < 0) {
            LOGE("Failed to initialize resampler");
            delete ctx;
            return nullptr;
        }

    }

    ctx->frame = av_frame_alloc();
    ctx->packet = av_packet_alloc();
    LOGE("audioCodecCtx  ctx->audioCodecCtx->channels=%d,ctx->frame->nb_samples=%d",
         ctx->audioCodecCtx->channels,  // 原始声道数
         ctx->frame->nb_samples);
    ctx->init();
    pthread_create(&ctx->videoThread, nullptr, videoPlayThread, ctx);
    pthread_create(&ctx->audioThread, nullptr, audioPlayThread, ctx);

    while (!ctx->abortRequest) {
        if (av_read_frame(ctx->formatCtx, ctx->packet) < 0)
            break;
        if (ctx->packet->stream_index == ctx->videoStreamIndex) {
            // 处理视频帧前检查同步状态
            pthread_mutex_lock(&ctx->audioClockMutex);
            double audioTime = ctx->audioClock;
            pthread_mutex_unlock(&ctx->audioClockMutex);

            double frameDuration = ctx->frameDuration / 1000000.0; // 转换为秒

            pthread_mutex_lock(&ctx->videoPendingMutex);
            if (ctx->pendingVideoDuration + frameDuration > audioTime + 0.1) {
                pthread_mutex_unlock(&ctx->videoPendingMutex);
                av_packet_unref(ctx->packet);
                continue; // 跳过超过阈值的视频帧
            }
            ctx->pendingVideoDuration += frameDuration;
            pthread_mutex_unlock(&ctx->videoPendingMutex);

            /* 自动sei帧解析// SEI检测逻辑
             for (int i = 0; i < ctx->packet->side_data_elems; i++) {
                 AVPacketSideData* sd = &ctx->packet->side_data[i];
                 if (sd->type == AV_PKT_DATA_SEI ) {
                     // 示例：打印SEI数据长度
                     LOGD("Got SEI data size: %d", sd->size)
                 }
             }*/
            // 手动动sei帧解析检查 SEI NAL 单元，H265不确定是否使用
            if (ctx->packet->data && ctx->packet->size >= 4) {
                if (containsSEI(ctx->packet->data, ctx->packet->size)) {
                    // 提取 SEI 数据
                    uint8_t *seiData = nullptr;
                    int seiSize = 0;
                    if (extractSEIData(ctx->packet->data, ctx->packet->size, &seiData, &seiSize)) {
                        LOGE("Got SEI data size: %d", seiSize)
                        free(seiData);
                    }
                }
            }

            // 处理视频帧
            if (avcodec_send_packet(ctx->videoCodecCtx, ctx->packet) == 0) {
                while (avcodec_receive_frame(ctx->videoCodecCtx, ctx->frame) == 0) {
                    // ===== 记录关键帧PTS =====
                    if (ctx->frame->key_frame || ctx->frame->pict_type == AV_PICTURE_TYPE_I) {
                        pthread_mutex_lock(&ctx->keyframeMutex);
                        ctx->lastKeyFramePts = ctx->frame->pts;
                        pthread_mutex_unlock(&ctx->keyframeMutex);
                    }
                    // =======================
                    AVFrame *frameCopy = av_frame_clone(ctx->frame);
                    pthread_mutex_lock(&ctx->videoMutex);
                    while (ctx->videoQueue.size() >= ctx->videoQueueMaxSize && !ctx->abortRequest) {
                        pthread_cond_wait(&ctx->videoCondNotFull, &ctx->videoMutex);
                    }
                    // LOGE("video %f " , frameCopy->pts * av_q2d(ctx->formatCtx->streams[ctx->videoStreamIndex]->time_base));
                    ctx->videoQueue.push(frameCopy);
                    //LOGE("videoQueue size: %d", ctx->videoQueue.size())
                    pthread_cond_signal(&ctx->videoCond);
                    pthread_mutex_unlock(&ctx->videoMutex);

                }
            }
        } else if (ctx->packet->stream_index == ctx->audioStreamIndex) {

            // 处理音频帧前检查未播放时长
            double duration = (double) ctx->frame->nb_samples / ctx->frame->sample_rate;

            pthread_mutex_lock(&ctx->audioPendingMutex);
            if (ctx->pendingAudioDuration + duration > 1.0) {
                pthread_mutex_unlock(&ctx->audioPendingMutex);
                LOGE("音频帧超过阈值")
                av_packet_unref(ctx->packet);
                continue; // 跳过超过阈值的音频帧
            }
            ctx->pendingAudioDuration += duration;
            pthread_mutex_unlock(&ctx->audioPendingMutex);
            // 处理音频帧
            if (avcodec_send_packet(ctx->audioCodecCtx, ctx->packet) == 0) {
                while (avcodec_receive_frame(ctx->audioCodecCtx, ctx->frame) == 0) {
                    int64_t delay = swr_get_delay(swr_ctx , ctx->frame->sample_rate);
                    // 预分配输出缓冲区
                    int max_out_samples = av_rescale_rnd(
                            ctx->frame->nb_samples+delay,
                            ctx->audioCodecCtx->sample_rate,
                            ctx->audioCodecCtx->sample_rate,
                            AV_ROUND_UP
                    );
                    uint8_t *output_buffer = nullptr;
                    int out_linesize;
                    av_samples_alloc(&output_buffer, &out_linesize,
                                     ctx->audioCodecCtx->channels,
                                     max_out_samples,
                                     AV_SAMPLE_FMT_FLT, 0);

                    int out_samples = swr_convert(
                            swr_ctx,
                            &output_buffer,
                            ctx->frame->nb_samples,
                            (const uint8_t **) ctx->frame->data,
                            ctx->frame->nb_samples
                    );
                    if (out_samples > 0) {
                        int bufSize = av_samples_get_buffer_size(
                                nullptr, ctx->audioCodecCtx->channels,
                                out_samples, AV_SAMPLE_FMT_FLT, 0
                        );
                        file.write(reinterpret_cast<const char*>(output_buffer), bufSize);
                        AudioData audioData;
                        audioData.data = (uint8_t *) av_malloc(bufSize);
                        memcpy(audioData.data, output_buffer, bufSize);
                        audioData.size = bufSize;
                        audioData.pts = ctx->frame->pts *
                                        av_q2d(ctx->formatCtx->streams[ctx->audioStreamIndex]->time_base);
                        pthread_mutex_lock(&ctx->audioMutex);
                        // 等待直到队列有空位
                        while (ctx->audioQueue.size() >= ctx->audioQueueMaxSize &&
                               !ctx->abortRequest) {
                            pthread_cond_wait(&ctx->audioCondNotFull, &ctx->audioMutex);
                        }
                        ctx->audioQueue.push(audioData);
                        pthread_cond_signal(&ctx->audioCond);
                        pthread_mutex_unlock(&ctx->audioMutex);
                    }
                }
            }
        }
        av_packet_unref(ctx->packet);
    }
    LOGE("END")
    file.close();
    // 等待播放线程结束
//    ctx->abortPlayback = true;
//    pthread_cond_broadcast(&ctx->videoCond);
//    pthread_cond_broadcast(&ctx->audioCond);
//    pthread_join(ctx->videoThread, nullptr);
//    pthread_join(ctx->audioThread, nullptr);
//    swr_free(&swr_ctx);
//    delete ctx;
    return nullptr;
}

// 新增视频播放线程
void *videoPlayThread(void *arg) {
    DecodeContext *ctx = (DecodeContext *) arg;
    using Clock = std::chrono::high_resolution_clock;

    Clock::time_point lastRenderTime = Clock::now();
    while (!ctx->abortPlayback) {
        pthread_mutex_lock(&ctx->videoMutex);
        while (ctx->videoQueue.empty() && !ctx->abortPlayback) {
            pthread_cond_wait(&ctx->videoCond, &ctx->videoMutex);
        }
        if (ctx->abortPlayback) {
            pthread_mutex_unlock(&ctx->videoMutex);
            break;
        }
        //LOGE("  videoPlayThread videoQueue size: %d", ctx->videoQueue.size())
        AVFrame *frame = ctx->videoQueue.front();
        ctx->videoQueue.pop();

        // 通知解码线程队列有空位
        pthread_cond_signal(&ctx->videoCondNotFull);
        pthread_mutex_unlock(&ctx->videoMutex);

        double pts = frame->pts * av_q2d(ctx->formatCtx->streams[ctx->videoStreamIndex]->time_base);

        // 更新未播放时长
        double duration = ctx->frameDuration / 1000000.0;

        pthread_mutex_lock(&ctx->videoPendingMutex);
        ctx->pendingVideoDuration -= duration;
        ctx->videoClock = pts;
        if (ctx->pendingVideoDuration < 0) ctx->pendingVideoDuration = 0;
        pthread_mutex_unlock(&ctx->videoPendingMutex);

        // 同步控制
        pthread_mutex_lock(&ctx->audioClockMutex);
        double audioTime = ctx->audioClock;
        //LOGE("audioTime:%f, pts:%f, syncDiff:%f", audioTime, pts, pts - audioTime)
        pthread_mutex_unlock(&ctx->audioClockMutex);
        LOGE("video2 %f audioTime=%f syncDiff=%f", pts, audioTime, audioTime - pts)
        double syncDiff = audioTime - pts;
        // 获取最后一个关键帧的PTS
        pthread_mutex_lock(&ctx->keyframeMutex);
        int64_t lastKeyPts = ctx->lastKeyFramePts;
        pthread_mutex_unlock(&ctx->keyframeMutex);

      /*  // 处理音频超前（0 < syncDiff < 1秒）
        if (syncDiff > 0 && syncDiff < 1.0) {
            // 动态调整播放速度
            double speedFactor = std::min(ctx->maxChaseSpeed, 1.0 + syncDiff * 0.5);
            ctx->frameAdjustFactor = 1.0 / speedFactor;
            // 激进模式：当差异超过0.7秒时丢弃非关键帧
            if (syncDiff > 0.7 && frame->pts != lastKeyPts) {
                av_frame_free(&frame);
                continue;
            }
        } else {
            ctx->frameAdjustFactor = 1.0; // 正常播放
        }*/

        // 计算动态帧间隔
        double adjustedInterval = (ctx->frameDuration / 1000000.0) * ctx->frameAdjustFactor;
        // 等待逻辑（带动态调整）
        auto now = Clock::now();
        double elapsed = std::chrono::duration<double>(now - lastRenderTime).count();
        double remainingTime = adjustedInterval - elapsed;
        LOGE("videoPlayThread wait %f", remainingTime)
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
        if (ctx->frameCallback) {
            auto renderStart = Clock::now();
            ctx->frameCallback->onFrameEncoded(frame);
            auto renderDuration = Clock::now() - renderStart;

            // 补偿渲染耗时
            lastRenderTime = Clock::now() + renderDuration;
        } else {
            lastRenderTime = Clock::now();
        }

        av_frame_free(&frame);
    }
    return nullptr;
}

// 新增音频播放线程
void *audioPlayThread(void *arg) {
    DecodeContext *ctx = (DecodeContext *) arg;
    while (!ctx->abortPlayback) {
        pthread_mutex_lock(&ctx->audioMutex);
        while (ctx->audioQueue.empty() && !ctx->abortPlayback) {
            pthread_cond_wait(&ctx->audioCond, &ctx->audioMutex);
        }
        if (ctx->abortPlayback) {
            pthread_mutex_unlock(&ctx->audioMutex);
            break;
        }
        AudioData audioData = ctx->audioQueue.front();
        ctx->audioQueue.pop();
        // 通知解码线程队列有空位
        pthread_cond_signal(&ctx->audioCondNotFull);
        pthread_mutex_unlock(&ctx->audioMutex);
        // 更新音频时钟
        int sampleRate = 44100;
        int sampleCount = audioData.size / (2 * sizeof(int16_t));
        double duration = static_cast<double>(sampleCount) / sampleRate;
        // 更新未播放时长
        pthread_mutex_lock(&ctx->audioPendingMutex);
        ctx->pendingAudioDuration -= duration;
        if (ctx->pendingAudioDuration < 0) ctx->pendingAudioDuration = 0;
        pthread_mutex_unlock(&ctx->audioPendingMutex);
        pthread_mutex_lock(&ctx->audioClockMutex);
        ctx->audioClock = audioData.pts;// +
        //(double) duration / ctx->audioCodecCtx->sample_rate; // 精确到已播放样本数
        LOGE("audio2 %f   audioData.pts=%f", ctx->audioClock, audioData.pts)
        pthread_mutex_unlock(&ctx->audioClockMutex);
        pthread_mutex_lock(&ctx->videoPendingMutex);
        double videoPendingTime = ctx->videoClock;
        pthread_mutex_unlock(&ctx->videoPendingMutex);

        double syncDiff = ctx->audioClock - videoPendingTime;

        // **如果音频快了，则延迟播放**
       /* if (syncDiff > 0.1&&videoPendingTime!=0) {
            LOGE("audio ahead=%f   ctx->audioClock=%f   videoPendingTime=%f", syncDiff,
                 ctx->audioClock, videoPendingTime)
            std::this_thread::sleep_for(std::chrono::duration<double>(syncDiff)); // 延迟音频播放
        }*/
        if (ctx->audioPlayer) {
            ctx->audioPlayer->writeData(audioData.data, audioData.size);
        }
        if (audioData.data) {
            av_free(audioData.data);
        }
    }
    return nullptr;
}

FFmpegEncodeStream::FFmpegEncodeStream() : decodeCtx(nullptr), threadId(0) {}

FFmpegEncodeStream::~FFmpegEncodeStream() {
    closeStream();
}

void FFmpegEncodeStream::setFrameCallback(FrameCallback *callback) {
    if (decodeCtx) decodeCtx->frameCallback = callback;
}


bool FFmpegEncodeStream::openStream(const char *filePath) {
    closeStream();
    decodeCtx = new DecodeContext();
    decodeCtx->filePath = av_strdup(filePath);

    if (pthread_create(&threadId, nullptr, decodeThread, decodeCtx) != 0) {
        delete decodeCtx;
        decodeCtx = nullptr;
        return false;
    }
    return true;
}

void FFmpegEncodeStream::closeStream() {
    if (decodeCtx) {
        decodeCtx->abortRequest = true;
        decodeCtx->abortPlayback = true;
        pthread_cond_broadcast(&decodeCtx->videoCondNotFull);
        pthread_cond_broadcast(&decodeCtx->audioCondNotFull);
        pthread_cond_broadcast(&decodeCtx->videoCond);
        pthread_cond_broadcast(&decodeCtx->audioCond);
        pthread_join(threadId, nullptr);
        delete decodeCtx;
        decodeCtx = nullptr;

    }
}