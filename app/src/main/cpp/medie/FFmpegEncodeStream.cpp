#include "FFmpegEncodeStream.h"
#include <pthread.h>
#include "Utils.cpp"


#define MAX_AUDIO_FRAME_SIZE 44100

bool containsSEI(const uint8_t *data, int size) {
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

        // 初始化音频重采样
        ctx->swrCtx = swr_alloc_set_opts(nullptr,
                                         AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,
                                         ctx->audioCodecCtx->channel_layout,
                                         ctx->audioCodecCtx->sample_fmt,
                                         ctx->audioCodecCtx->sample_rate, 0, nullptr);
        swr_init(ctx->swrCtx);
    }

    ctx->frame = av_frame_alloc();
    ctx->packet = av_packet_alloc();
    uint8_t *audioBuffer = nullptr;
    int audioBufferSize = 0;

    while (!ctx->abortRequest) {
        if (av_read_frame(ctx->formatCtx, ctx->packet) < 0)
            break;

        if (ctx->packet->stream_index == ctx->videoStreamIndex) {
            /* 自动sei帧解析// SEI检测逻辑
             for (int i = 0; i < ctx->packet->side_data_elems; i++) {
                 AVPacketSideData* sd = &ctx->packet->side_data[i];
                 if (sd->type == AV_PKT_DATA_SEI ) {
                     // 示例：打印SEI数据长度
                     LOGD("Got SEI data size: %d", sd->size)
                 }
             }*/
            // 手动动sei帧解析检查 SEI NAL 单元，H265不确定是否使用
            if (containsSEI(ctx->packet->data, ctx->packet->size)) {
                // 提取 SEI 数据
                uint8_t *seiData = nullptr;
                int seiSize = 0;
                if (extractSEIData(ctx->packet->data, ctx->packet->size, &seiData, &seiSize)) {
                    LOGD("Got SEI data size: %d", seiSize)
                    free(seiData);
                }
            }
            // 处理视频帧
            if (avcodec_send_packet(ctx->videoCodecCtx, ctx->packet) == 0) {
                while (avcodec_receive_frame(ctx->videoCodecCtx, ctx->frame) == 0) {
                    if (ctx->frameCallback) {
                        ctx->frameCallback->onFrameEncoded(ctx->frame);
                    }
                }
            }
        } else if (ctx->packet->stream_index == ctx->audioStreamIndex) {
            // 处理音频帧
            if (avcodec_send_packet(ctx->audioCodecCtx, ctx->packet) == 0) {
                while (avcodec_receive_frame(ctx->audioCodecCtx, ctx->frame) == 0) {
                    // 重采样音频
                    int outSamples = swr_get_out_samples(ctx->swrCtx, ctx->frame->nb_samples);
                    int bufSize = av_samples_get_buffer_size(nullptr, 2, outSamples,
                                                             AV_SAMPLE_FMT_S16, 1);
                    if (audioBufferSize < bufSize) {
                        av_free(audioBuffer);
                        audioBuffer = static_cast<uint8_t *>(av_malloc(bufSize));
                        audioBufferSize = bufSize;
                    }

                    uint8_t *outBuffers[] = {audioBuffer};
                    swr_convert(ctx->swrCtx, outBuffers, outSamples,
                                (const uint8_t **) ctx->frame->data, ctx->frame->nb_samples);

                    if (ctx->audioCallback) {
                        ctx->audioCallback->onAudioEncoded(audioBuffer, bufSize);
                    }
                }
            }
        }

        av_packet_unref(ctx->packet);
    }

    // 清理资源
    av_free(audioBuffer);
    delete ctx;
    return nullptr;
}

FFmpegEncodeStream::FFmpegEncodeStream() : decodeCtx(nullptr), threadId(0) {}

FFmpegEncodeStream::~FFmpegEncodeStream() {
    closeStream();
}

void FFmpegEncodeStream::setFrameCallback(FrameCallback *callback) {
    //std::lock_guard<std::mutex> lock(mutex);
    if (decodeCtx) decodeCtx->frameCallback = callback;
}

void FFmpegEncodeStream::setAudioCallback(AudioCallback *callback) {
    // std::lock_guard<std::mutex> lock(mutex);
    if (decodeCtx) decodeCtx->audioCallback = callback;
}

bool FFmpegEncodeStream::openStream(const char *filePath) {
    //std::lock_guard<std::mutex> lock(mutex);
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
    // std::lock_guard<std::mutex> lock(mutex);
    if (decodeCtx) {
        decodeCtx->abortRequest = true;
        pthread_join(threadId, nullptr);
        delete decodeCtx;
        decodeCtx = nullptr;
    }
}