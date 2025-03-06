#include "FFmpegEncodeStream.h"
#include <pthread.h>
#include "Utils.cpp"


#define MAX_AUDIO_FRAME_SIZE 44100



void* decodeThread(void* arg) {
    DecodeContext *ctx = static_cast<DecodeContext*>(arg);
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
            ctx->audioCodecCtx->channel_layout, ctx->audioCodecCtx->sample_fmt,
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
                        audioBuffer = static_cast<uint8_t*>(av_malloc(bufSize));
                        audioBufferSize = bufSize;
                    }

                    uint8_t *outBuffers[] = {audioBuffer};
                    swr_convert(ctx->swrCtx, outBuffers, outSamples, 
                              (const uint8_t**)ctx->frame->data, ctx->frame->nb_samples);

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