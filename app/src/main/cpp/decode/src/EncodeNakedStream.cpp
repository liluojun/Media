//
// Created by hjt on 2025/4/11.
//

#include "../include/EncodeNakedStream.h"

int get_nal_type(const uint8_t *data, int codecId) {
    if (codecId == AV_CODEC_ID_H264) {
        return data[0] & 0x1F;  // H264: lower 5 bits
    } else if (codecId == AV_CODEC_ID_HEVC) {
        return (data[0] >> 1) & 0x3F;  // H265: bits 1-6
    }
    return -1;
}

// 将一帧 H264 AnnexB 格式转为 AVCC 格式（用于 MediaCodec 解码）
std::vector<uint8_t> convertAnnexBToAVCC(const uint8_t *data, size_t size) {
    std::vector<uint8_t> avccData;
    size_t pos = 0;
    while (pos + 3 < size) {
        // 查找起始码
        size_t startCodeLen = 0;
        if (data[pos] == 0x00 && data[pos + 1] == 0x00) {
            if (data[pos + 2] == 0x01) {
                startCodeLen = 3;
            } else if (data[pos + 2] == 0x00 && data[pos + 3] == 0x01) {
                startCodeLen = 4;
            }
        }

        if (startCodeLen == 0) {
            pos++;
            continue;
        }

        size_t nal_start = pos + startCodeLen;
        size_t nal_end = size;

        // 查找下一个起始码，作为当前 NAL 结束
        for (size_t i = nal_start; i + 3 < size; ++i) {
            if ((data[i] == 0x00 && data[i + 1] == 0x00 &&
                 ((data[i + 2] == 0x01) || (data[i + 2] == 0x00 && data[i + 3] == 0x01)))) {
                nal_end = i;
                break;
            }
        }

        size_t nal_size = nal_end - nal_start;

        // 添加 NAL 长度前缀（4 字节）
        avccData.push_back((nal_size >> 24) & 0xFF);
        avccData.push_back((nal_size >> 16) & 0xFF);
        avccData.push_back((nal_size >> 8) & 0xFF);
        avccData.push_back(nal_size & 0xFF);

        // 添加 NALU 数据
        avccData.insert(avccData.end(), data + nal_start, data + nal_end);

        pos = nal_end;
    }

    return avccData;
}

// 从 I 帧中提取 VPS/SPS/PPS（支持 H264/H265）
CodecExtraData parseSpsPpsVpsFromIFrame(const uint8_t *data, size_t size, AVCodecID codecId) {
    CodecExtraData result;

    size_t pos = 0;
    while (pos + 4 < size) {
        // 查找起始码
        size_t start = pos;
        if (data[pos] == 0x00 && data[pos + 1] == 0x00) {
            if (data[pos + 2] == 0x01) {
                pos += 3;
            } else if (data[pos + 2] == 0x00 && data[pos + 3] == 0x01) {
                pos += 4;
            } else {
                pos++;
                continue;
            }
        } else {
            pos++;
            continue;
        }

        size_t nal_start = pos;
        // 查找下一个起始码作为当前 NALU 的结束
        size_t nal_end = size;
        for (size_t i = pos; i + 3 < size; ++i) {
            if ((data[i] == 0x00 && data[i + 1] == 0x00 &&
                 ((data[i + 2] == 0x01) || (data[i + 2] == 0x00 && data[i + 3] == 0x01)))) {
                nal_end = i;
                break;
            }
        }

        int nal_type = get_nal_type(&data[nal_start], codecId);
        const uint8_t *nal_ptr = data + nal_start;
        size_t nal_size = nal_end - nal_start;

        if (codecId == AV_CODEC_ID_H264) {
            if (nal_type == 7) { // SPS
                result.sps.assign(nal_ptr, nal_ptr + nal_size);
            } else if (nal_type == 8) { // PPS
                result.pps.assign(nal_ptr, nal_ptr + nal_size);
            }
        } else if (codecId == AV_CODEC_ID_HEVC) {
            if (nal_type == 32) { // VPS
                result.vps.assign(nal_ptr, nal_ptr + nal_size);
            } else if (nal_type == 33) { // SPS
                result.sps.assign(nal_ptr, nal_ptr + nal_size);
            } else if (nal_type == 34) { // PPS
                result.pps.assign(nal_ptr, nal_ptr + nal_size);
            }
        }

        pos = nal_end;
    }

    return result;
}

bool parseHeader(const std::vector<uint8_t> &header, NakedFrameData *data) {
    try {
        data->frametype = *reinterpret_cast<const int64_t *>(&header[4]);

        data->pts = *reinterpret_cast<const int64_t *>(&header[12]);
        data->size = *reinterpret_cast<const int *>(&header[20]);
        data->width = *reinterpret_cast<const int16_t *>(&header[24]);
        data->height = *reinterpret_cast<const int16_t *>(&header[26]);
        data->data = new uint8_t[data->size];
        int codeId = *reinterpret_cast<const int8_t *>(&header[28]);
        switch (codeId) {
            case 0:
                data->codecId = AV_CODEC_ID_H264;
                break;
            case 0x10:
                data->codecId = AV_CODEC_ID_H265;
                break;
            default:
                data->codecId = AV_CODEC_ID_H264;
                break;
        }
        return true;
    } catch (const std::exception &e) {
        LOGE("解析头失败");
        return false;
    }

}

void getAndroidCodec(AVCodecID codecId, char *codecName) {
    switch (codecId) {
        case AV_CODEC_ID_H264:
            strcpy(codecName, "h264_mediacodec");
            break;
        case AV_CODEC_ID_H265:
            strcpy(codecName, "hevc_mediacodec");
            break;
        default:
            strcpy(codecName, "");
            break;
    }
}

void buildAvccExtradata(const std::vector<uint8_t> &sps,
                        const std::vector<uint8_t> &pps, std::vector<uint8_t> &extradata) {


    if (sps.size() < 4) return;

    // AVCC header
    extradata.push_back(0x01);               // configurationVersion
    extradata.push_back(sps[1]);             // profile_idc
    extradata.push_back(sps[2]);             // profile_compat
    extradata.push_back(sps[3]);             // level_idc
    extradata.push_back(
            0xFF);               // 6 bits reserved + 2 bits lengthSizeMinusOne (4 bytes)

    // SPS count
    extradata.push_back(0xE1);               // 3 bits reserved + 5 bits numOfSPS (1)
    extradata.push_back((sps.size() >> 8) & 0xFF); // SPS size high
    extradata.push_back(sps.size() & 0xFF);        // SPS size low
    extradata.insert(extradata.end(), sps.begin(), sps.end());

    // PPS count
    extradata.push_back(0x01);               // numOfPPS = 1
    extradata.push_back((pps.size() >> 8) & 0xFF); // PPS size high
    extradata.push_back(pps.size() & 0xFF);        // PPS size low
    extradata.insert(extradata.end(), pps.begin(), pps.end());

    return;
}

// 辅助函数：设置编解码器extra data
void setCodecExtraData(AVCodecContext *codecCtx, NakedFrameData *data) {
    CodecExtraData extra = parseSpsPpsVpsFromIFrame(data->data, data->size, data->codecId);
    std::vector<uint8_t> extradata;
    auto appendNalWithLengthPrefix = [](std::vector<uint8_t> &dest,
                                        const std::vector<uint8_t> &nal) {
        uint32_t len = nal.size();
        dest.push_back((len >> 24) & 0xFF);
        dest.push_back((len >> 16) & 0xFF);
        dest.push_back((len >> 8) & 0xFF);
        dest.push_back(len & 0xFF);
        dest.insert(dest.end(), nal.begin(), nal.end());
    };

    if (data->codecId == AV_CODEC_ID_H264) {
        buildAvccExtradata(extra.sps, extra.pps, extradata);
    } else if (data->codecId == AV_CODEC_ID_HEVC) {
        if (!extra.vps.empty()) appendNalWithLengthPrefix(extradata, extra.vps);
        if (!extra.sps.empty()) appendNalWithLengthPrefix(extradata, extra.sps);
        if (!extra.pps.empty()) appendNalWithLengthPrefix(extradata, extra.pps);
    }

    codecCtx->extradata = (uint8_t *) av_malloc(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(codecCtx->extradata, extradata.data(), extradata.size());
    codecCtx->extradata_size = extradata.size();
}

// 辅助函数：获取硬件像素格式
enum AVPixelFormat getHardwarePixelFormat(const AVCodec *videoCodec) {
    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(videoCodec, i);
        if (!config) break;

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == AV_HWDEVICE_TYPE_MEDIACODEC) {
            return config->pix_fmt;
        }
    }
    return AV_PIX_FMT_NONE;
}

// 静态回调函数
static enum AVPixelFormat
get_hw_format_callback(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    if (!ctx || !ctx->opaque) return AV_PIX_FMT_NONE;

    enum AVPixelFormat hw_pix_fmt = *(enum AVPixelFormat *) ctx->opaque;

    for (; *pix_fmts != AV_PIX_FMT_NONE; pix_fmts++) {
        if (*pix_fmts == hw_pix_fmt) {
            return *pix_fmts;
        }
    }
    return AV_PIX_FMT_NONE;
}

// 初始化软解器函数
int initSoftwareDecoder(InitContext *ctx, NakedFrameData *data) {
    const AVCodec *videoCodec = avcodec_find_decoder(data->codecId);
    if (!videoCodec) {
        LOGE("Software video codec not found");
        return -2;
    }

    ctx->videoDecodeCtx->videoCodecCtx = avcodec_alloc_context3(videoCodec);
    if (!ctx->videoDecodeCtx->videoCodecCtx) {
        LOGE("Failed to allocate software codec context");
        return -3;
    }

    // 配置软解参数
    ctx->videoDecodeCtx->videoCodecCtx->width = data->width;
    ctx->videoDecodeCtx->videoCodecCtx->height = data->height;
    ctx->videoDecodeCtx->videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->videoDecodeCtx->user_time_base = (AVRational) {1, 25};
    ctx->videoDecodeCtx->frame_count = 0;

    // 打开软解器
    if (avcodec_open2(ctx->videoDecodeCtx->videoCodecCtx, videoCodec, NULL) < 0) {
        LOGE("Failed to open software codec");
        avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
        return -4;
    }

    LOGI("Software decoder initialized successfully");
    ctx->isSoftOrHardDecod= false;
    return 0;
}
// 初始化硬解器函数
#ifdef __ANDROID__

bool initHardwareDecoder(InitContext *ctx, NakedFrameData *data, const AVCodec *videoCodec) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    ctx->videoDecodeCtx->videoCodecCtx = avcodec_alloc_context3(videoCodec);
    if (!ctx->videoDecodeCtx->videoCodecCtx) {
        LOGE("Failed to allocate video codec context");
        return false;
    }

    // 配置编解码器参数
    ctx->videoDecodeCtx->videoCodecCtx->width = data->width;
    ctx->videoDecodeCtx->videoCodecCtx->height = data->height;
    ctx->videoDecodeCtx->user_time_base = (AVRational) {1, 25};
    ctx->videoDecodeCtx->frame_count = 0;

    // 设置extra data
    if (data->frametype == PktIFrames) {
        setCodecExtraData(ctx->videoDecodeCtx->videoCodecCtx, data);
    } else {
        LOGW("Non-I-frame cannot initialize hardware decoder");
        return false;
    }

    // 获取硬件像素格式
    ctx->videoDecodeCtx->hw_pix_fmt = getHardwarePixelFormat(videoCodec);
    if (ctx->videoDecodeCtx->hw_pix_fmt == AV_PIX_FMT_NONE) {
        LOGE("No suitable hardware pixel format found");
        return false;
    }

    // 创建硬件设备上下文
    AVBufferRef *hw_device_ctx = nullptr;
    if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_MEDIACODEC, nullptr, nullptr, 0) <
        0) {
        LOGE("Failed to create HW device context");
        return false;
    }
    ctx->videoDecodeCtx->videoCodecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    av_buffer_unref(&hw_device_ctx);
    // 设置格式回调
    // 设置 get_format 回调 + opaque 传参
    enum AVPixelFormat *pix_fmt_ptr = (enum AVPixelFormat *) av_malloc(sizeof(enum AVPixelFormat));
    *pix_fmt_ptr = ctx->videoDecodeCtx->hw_pix_fmt;
    ctx->videoDecodeCtx->videoCodecCtx->opaque = pix_fmt_ptr;
    ctx->videoDecodeCtx->videoCodecCtx->get_format = get_hw_format_callback;
    // 配置硬件解码选项
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "output_mode", "pixel_buffer", 0);
    // 打开编解码器
    int ret = avcodec_open2(ctx->videoDecodeCtx->videoCodecCtx, videoCodec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        LOGE("avcodec_open2 failed: %s", errbuf)
        // 清理资源
        av_freep(&ctx->videoDecodeCtx->videoCodecCtx->opaque);
        avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
        return false;
    }

    LOGI("Hardware decoder initialized successfully");
    return true;
}

#endif

int initVideoAVCodec(InitContext *ctx, const AVCodec *videoCodec, NakedFrameData *data,
                     AVCodecID *lastAVCodecID) {

    // 检查是否需要重新初始化编解码器
    bool needReinit = !ctx->videoDecodeCtx->videoCodecCtx ||
                      *lastAVCodecID != data->codecId ||
                      ctx->videoDecodeCtx->videoCodecCtx->width != data->width ||
                      ctx->videoDecodeCtx->videoCodecCtx->height != data->height;

    if (!needReinit) return 0;
    // 释放现有编解码器上下文
    if (ctx->videoDecodeCtx->videoCodecCtx) {
        if (ctx->videoDecodeCtx->videoCodecCtx->opaque) {
            av_freep(&ctx->videoDecodeCtx->videoCodecCtx->opaque);
        }
        avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
    }
    // 硬解模式处理
    if (ctx->isSoftOrHardDecod) {
#ifdef __ANDROID__
        bool hwSuccess = false;
        char codecName[64] = {0};
        getAndroidCodec(data->codecId, codecName);

        // 尝试硬解
        if (strlen(codecName) > 0) {
            const AVCodec *videoCodec = avcodec_find_decoder_by_name(codecName);
            if (videoCodec) {
                hwSuccess = initHardwareDecoder(ctx, data, videoCodec);
            }
        }

        // 硬解失败时回退软解
        if (!hwSuccess) {
            LOGW("Hardware decoding failed, falling back to software");
            *lastAVCodecID = data->codecId;
            return initSoftwareDecoder(ctx, data);
        }
#else
        // 非Android平台直接使用软解
         *lastAVCodecID = data->codecId;
        return initSoftwareDecoder(ctx, data);
#endif
    } else {
        // 纯软解模式
        *lastAVCodecID = data->codecId;
        return initSoftwareDecoder(ctx, data);
    }
    *lastAVCodecID = data->codecId;
    return 0;
}

/*if (videoCodec != nullptr || (*lastAVCodecID == AV_CODEC_ID_NONE) ||
    (*lastAVCodecID != AV_CODEC_ID_NONE && *lastAVCodecID != data->codecId) ||
    (ctx->videoDecodeCtx->videoCodecCtx == nullptr) ||
    (ctx->videoDecodeCtx->videoCodecCtx->width != data->width) ||
    (ctx->videoDecodeCtx->videoCodecCtx->height != data->height)) {
    if (ctx->videoDecodeCtx->videoCodecCtx != nullptr) {
        avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
    }
    if (!ctx->isSoftOrHardDecod) {
        videoCodec = avcodec_find_decoder(data->codecId);
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
    } else {
#ifdef __ANDROID__
        char codecName[64] = {0};
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        getAndroidCodec(data->codecId, codecName);
        if (strlen(codecName) > 0) {
            LOGE("yingjie")
            videoCodec = avcodec_find_decoder_by_name(codecName);
        }
        if (!videoCodec) {
            videoCodec = avcodec_find_decoder(data->codecId);
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
        } else {
            ctx->videoDecodeCtx->videoCodecCtx = avcodec_alloc_context3(videoCodec);
            if (data->frametype != PktIFrames) {
                LOGE("非I帧无法搜寻sps与pps")
                return -5;
            } else {
                CodecExtraData extra = parseSpsPpsVpsFromIFrame(data->data, data->size,
                                                                data->codecId);
                std::vector<uint8_t> extradata;
                auto appendNalWithLengthPrefix = [](std::vector<uint8_t> &dest,
                                                    const std::vector<uint8_t> &nal) {
                    uint32_t len = nal.size();
                    dest.push_back((len >> 24) & 0xFF);
                    dest.push_back((len >> 16) & 0xFF);
                    dest.push_back((len >> 8) & 0xFF);
                    dest.push_back(len & 0xFF);
                    dest.insert(dest.end(), nal.begin(), nal.end());
                };
                if (data->codecId == AV_CODEC_ID_H264) {
                    buildAvccExtradata(extra.sps, extra.pps, extradata);
                } else if (data->codecId == AV_CODEC_ID_HEVC) {
                    if (!extra.vps.empty()) appendNalWithLengthPrefix(extradata, extra.vps);
                    if (!extra.sps.empty()) appendNalWithLengthPrefix(extradata, extra.sps);
                    if (!extra.pps.empty()) appendNalWithLengthPrefix(extradata, extra.pps);
                }
                ctx->videoDecodeCtx->videoCodecCtx->extradata = (uint8_t *) av_malloc(
                        extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE);
                memcpy(ctx->videoDecodeCtx->videoCodecCtx->extradata, extradata.data(),
                       extradata.size());
                ctx->videoDecodeCtx->videoCodecCtx->extradata_size = extradata.size();
            }

            for (int i = 0;; i++) {
                const AVCodecHWConfig *config = avcodec_get_hw_config(videoCodec, i);
                if (!config)
                    break;
                const char *name = av_get_pix_fmt_name(config->pix_fmt);
                LOGE("pix_fmt=%d name=%s\n", config->pix_fmt, name);
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                    config->device_type == AV_HWDEVICE_TYPE_MEDIACODEC) {
                    hw_pix_fmt = config->pix_fmt; // ✅ 初始化正确的格式
                    break;
                }
            }
            // 创建硬件设备上下文
            AVBufferRef *hw_device_ctx = nullptr;
            if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_MEDIACODEC, nullptr,
                                       nullptr, 0) < 0) {
                LOGE("创建 HW 设备上下文失败");
                return -6;
            }
            ctx->videoDecodeCtx->videoCodecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            av_buffer_unref(&hw_device_ctx);

            // 设置格式回调，用于确定硬件解码格式
            ctx->videoDecodeCtx->videoCodecCtx->get_format = [](AVCodecContext *s,
                                                                const enum AVPixelFormat *pix_fmts) {
                for (; *pix_fmts != AV_PIX_FMT_NONE; pix_fmts++) {
                    if (*pix_fmts == hw_pix_fmt) {
                        return *pix_fmts;
                    }
                }
                return AV_PIX_FMT_NONE;
            };
            ctx->videoDecodeCtx->videoCodecCtx->width = data->width;
            ctx->videoDecodeCtx->videoCodecCtx->height = data->height;
            ctx->videoDecodeCtx->user_time_base = (AVRational) {1, 25};
            ctx->videoDecodeCtx->frame_count = 0;
            AVDictionary *opts = NULL;
            // 设置输出模式为 pixel_buffer（像素缓冲区）
            av_dict_set(&opts, "output_mode", "pixel_buffer", 0);
            int ret = avcodec_open2(ctx->videoDecodeCtx->videoCodecCtx, videoCodec, &opts);
            av_strerror(ret, errbuf, sizeof(errbuf));
            LOGE("avcodec_open2 , ret=%d, reason=%s", ret, errbuf);
            if (ret < 0) {
                avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
                ctx->videoInitFailClean();
                return -4;
            }
        }
#else
        videoCodec = avcodec_find_decoder(data->codecId);
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
#endif
    }
    *lastAVCodecID = data->codecId;

}
return 0;
}*/

void videoThread(InitContext *ctx) {
    int count = 0;
    const AVCodec *videoCodec = nullptr;
    AVCodecID lastAVCodecID = AV_CODEC_ID_NONE;
    ctx->videoDecodeCtx->calculateFrameRateNaked();
    while (!ctx->videoDecodeCtx->abortRequest) {
        pthread_mutex_lock(&ctx->readVideoMutex);
        while (ctx->videoReadDecode.empty() && !ctx->videoDecodeCtx->abortRequest) {
            pthread_cond_wait(&ctx->readVideoCond, &ctx->readVideoMutex);
        }
        LOGE("1111")
        if (ctx->videoDecodeCtx->abortRequest || ctx->videoReadDecode.empty()) {
            pthread_mutex_unlock(&ctx->readVideoMutex);
            break;
        }
        NakedFrameData *packet = ctx->videoReadDecode.front();
        ctx->videoReadDecode.pop();
        pthread_mutex_unlock(&ctx->readVideoMutex);
        if (ctx->videoDecodeCtx->abortRequest) {
            delete packet;
            break;
        }
        if (packet->frameNo == -1) {
            avcodec_send_packet(ctx->videoDecodeCtx->videoCodecCtx, NULL);
        } else {
            int ret = initVideoAVCodec(ctx, videoCodec, packet, &lastAVCodecID);
            if (ret < 0) {
                LOGE("initVideoAVCodec error=%d", ret)
                delete packet;
                continue;
            }
            LOGE("2222")
            AVPacket *pkt = av_packet_alloc();
            pkt->data = packet->data;
            pkt->size = packet->size;
            if (ctx->isSoftOrHardDecod) {
                std::vector<uint8_t> avccFrame = convertAnnexBToAVCC(packet->data, packet->size);
                av_new_packet(pkt, avccFrame.size());
                memcpy(pkt->data, avccFrame.data(), avccFrame.size());
            }
            ret = avcodec_send_packet(ctx->videoDecodeCtx->videoCodecCtx, pkt);
            av_packet_free(&pkt);
            if (ret < 0 && ret != AVERROR(EAGAIN)) {
                LOGE("Error sending packet: %s", av_err2str(ret));
                continue;
            }
        }

        delete packet;

        LOGE("3333")
        AVFrame *frame = av_frame_alloc();
        while (avcodec_receive_frame(ctx->videoDecodeCtx->videoCodecCtx, frame) == 0 &&
               !ctx->videoDecodeCtx->abortRequest) {
            LOGE("4444")
            AVFrame *finalFrame = frame;
            // 如果是硬解帧，进行内存拷贝到系统内存（sw_frame）
            if (frame->format == ctx->videoDecodeCtx->hw_pix_fmt) {
                LOGE("当前为硬件帧，使用av_hwframe_transfer_data转为系统内存")
                AVFrame *sw_frame = av_frame_alloc();
                if (!sw_frame) {
                    LOGE("Failed to alloc sw_frame");
                    av_frame_unref(frame);
                    continue;
                }
                int err = av_hwframe_transfer_data(sw_frame, frame, 0);
                if (err < 0) {
                    LOGE("av_hwframe_transfer_data failed: %s", av_err2str(err));
                    av_frame_free(&sw_frame);
                    av_frame_unref(frame);
                    continue;
                }

                finalFrame = sw_frame;  // 用 sw_frame 代替原 frame（即数据在系统内存）
                av_frame_unref(frame);  // 原始硬件帧可以释放
                if (finalFrame != frame) {
                    av_frame_free(&frame);  // 真正释放原始 frame
                }
            }
            struct SwsContext *sws_ctx = sws_getContext(
                    frame->width, frame->height, AV_PIX_FMT_NV12,   // 输入格式
                    frame->width, frame->height, AV_PIX_FMT_YUV420P, // 输出格式
                    SWS_BILINEAR, NULL, NULL, NULL);
            AVFrame *frameCopy = nullptr;
            if (frame->format == AV_PIX_FMT_NV12 || frame->format == AV_PIX_FMT_NV21) {
                AVFrame *yuv420p_frame = av_frame_alloc();
                yuv420p_frame->format = AV_PIX_FMT_YUV420P;
                yuv420p_frame->width = finalFrame->width;
                yuv420p_frame->height = finalFrame->height;
                av_frame_get_buffer(yuv420p_frame, 32);
                // 转换
                sws_scale(
                        sws_ctx,
                        frame->data, frame->linesize,
                        0, frame->height,
                        yuv420p_frame->data, yuv420p_frame->linesize);

                sws_freeContext(sws_ctx);
                frameCopy = av_frame_clone(yuv420p_frame);
            } else {
                frameCopy = av_frame_clone(finalFrame);
            }
            pthread_mutex_lock(&ctx->videoDecodeCtx->viedoDecodeMutex);
            while (ctx->videoDecodeCtx->videoDecodeQueue.size() >=
                   ctx->videoDecodeCtx->MAX_VIDEO_FRAME &&
                   !ctx->videoDecodeCtx->abortRequest) {
                pthread_cond_wait(&ctx->videoDecodeCtx->viedoDecodeFullCond,
                                  &ctx->videoDecodeCtx->viedoDecodeMutex);
            }
            ctx->videoDecodeCtx->videoDecodeQueue.push(frameCopy);
            count++;
            LOGE("count=%d", count);
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
        int64_t deltaUs = framePtsUs - ctx->syncClock->getVideoPlayTimeUs();
        if (deltaUs > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(deltaUs));
        }

        // 执行渲染
        if (ctx->videoDecodeCtx->frameCallback) {
            ctx->videoDecodeCtx->frameCallback->onFrameEncoded(frame);
        }

        av_frame_free(&frame);
    }
    return;
}

int initAudioAVCodec(InitContext *ctx, const AVCodec *audioCodec, NakedFrameData *data) {
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
        ctx->audioDecodeCtx->audioDecodeCtx->channels = AUDIO_CHANNELS;
        ctx->audioDecodeCtx->audioDecodeCtx->sample_rate = AUDIO_SAMPLE_RATE;
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
    const AVCodec *audioCodec = nullptr;
    while (!ctx->audioDecodeCtx->abortRequest) {
        pthread_mutex_lock(&ctx->readAudioMutex);
        while (ctx->audioReadDecode.empty() && !ctx->audioDecodeCtx->abortRequest) {
            pthread_cond_wait(&ctx->readAudioCond, &ctx->readAudioMutex);
        }
        if (ctx->audioDecodeCtx->abortRequest || ctx->audioReadDecode.empty()) {
            pthread_mutex_unlock(&ctx->readAudioMutex);
            break;
        }
        NakedFrameData *packet = ctx->audioReadDecode.front();
        ctx->audioReadDecode.pop();
        pthread_mutex_unlock(&ctx->readAudioMutex);
        if (ctx->audioDecodeCtx->abortRequest) {
            delete packet;
            break;
        }
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
            if (ctx->syncClock->getPlaybackSpeed() != 1) {
                sonicWriteShortToStream(ctx->audioDecodeCtx->sonicStream,
                                        (int16_t *) frame->data[0],
                                        frame->nb_samples * frame->channels);
                int16_t outBuf[8192];
                int samplesOut = 0;
                while ((samplesOut = sonicReadShortFromStream(ctx->audioDecodeCtx->sonicStream,
                                                              outBuf, 8192)) > 0) {
                    int dataSize = samplesOut * sizeof(int16_t);
                    uint8_t *buffer = (uint8_t *) malloc(dataSize);
                    memcpy(buffer, outBuf, dataSize);
                    nakedFrameData.data = buffer;
                    nakedFrameData.size = dataSize;
                    // 计算变速后时长，除以 speed
                    int64_t durationUs = static_cast<int64_t>((samplesOut / frame->channels) *
                                                              AV_TIME_BASE /
                                                              ctx->syncClock->getPlaybackSpeed());
                    nakedFrameData.pts = ctx->audioDecodeCtx->pts;
                    ctx->audioDecodeCtx->pts += durationUs;
                    // LOGE("倍速播放 %d   %d",nakedFrameData.pts,ctx->audioDecodeCtx->pts)
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

            } else {
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
}

void audioRenderThread(InitContext *ctx) {
    while (!ctx->audioDecodeCtx->playRequest) {
        pthread_mutex_lock(&ctx->audioDecodeCtx->audioDecodeMutex);
        while (ctx->audioDecodeCtx->audioDecodeQueue.empty() && !ctx->audioDecodeCtx->playRequest) {
            pthread_cond_wait(&ctx->audioDecodeCtx->audioDecodeEmptyCond,
                              &ctx->audioDecodeCtx->audioDecodeMutex);
        }
        if (ctx->audioDecodeCtx->abortRequest || ctx->audioDecodeCtx->audioDecodeQueue.empty()) {
            pthread_mutex_unlock(&ctx->audioDecodeCtx->audioDecodeMutex);
            break;
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
                        pthread_mutex_lock(&ctx->readVideoMutex);
                        NakedFrameData *data = new NakedFrameData();
                        data->frameNo = -1;
                        ctx->videoReadDecode.push(data);
                        pthread_cond_signal(&ctx->readVideoCond);
                        pthread_mutex_unlock(&ctx->readVideoMutex);
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

void custom_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
    if (level <= av_log_get_level()) {
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), fmt, vl);
        __android_log_print(ANDROID_LOG_ERROR, "ffmpeg", "%s", buffer);
    }
}


void saveSurface(InitContext *decodeCtx, jobject surface) {
    JavaVM *javaVm = (JavaVM *) av_jni_get_java_vm(NULL); // 注意这里返回值赋值
    if (!javaVm) {
        // 获取失败，处理错误
        return;
    }
    JNIEnv *env = nullptr;
    if (javaVm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        // 当前线程未附加 JVM，尝试附加线程
        if (javaVm->AttachCurrentThread(&env, NULL) != JNI_OK) {
            // 附加失败，处理错误
            return;
        }
    }
    // 使用 env 和 surface 创建 SurfaceHolder
    decodeCtx->surfaceHolder = new SurfaceHolder(javaVm, env, surface);
}

bool EncodeNakedStream::openStream(const char *filePath) {
    try {
        av_log_set_callback(custom_log_callback);
        av_log_set_level(AV_LOG_DEBUG);
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
        //saveSurface(decodeCtx, surface);

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
        if (decodeCtx->videoDecodeCtx) {
            decodeCtx->videoDecodeCtx->abortRequest = true;
            decodeCtx->videoDecodeCtx->renderRequest = true;

            // 唤醒等待帧队列的线程
            pthread_cond_broadcast(&decodeCtx->videoDecodeCtx->viedoDecodeFullCond);
            pthread_cond_broadcast(&decodeCtx->videoDecodeCtx->viedoDecodeEmptyCond);
        }
        if (decodeCtx->audioDecodeCtx) {
            decodeCtx->audioDecodeCtx->abortRequest = true;
            decodeCtx->audioDecodeCtx->playRequest = true;
            // 唤醒等待音频队列的线程
            pthread_cond_broadcast(&decodeCtx->audioDecodeCtx->audioDecodeFullCond);
            pthread_cond_broadcast(&decodeCtx->audioDecodeCtx->audioDecodeEmptyCond);
        }

        // 唤醒读线程中的 cond wait
        pthread_cond_broadcast(&decodeCtx->readVideoCond);
        pthread_cond_broadcast(&decodeCtx->readAudioCond);
        if (workerThread.joinable()) {
            workerThread.join(); // 替换 pthread_join
        }
        if (decodeCtx->surfaceHolder) {
            delete decodeCtx->surfaceHolder;
            decodeCtx->surfaceHolder = nullptr;
        }
        delete decodeCtx;
        decodeCtx = nullptr;
        LOGE("closeStream")
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
        if ((speed >= 0.5 || speed <= 2) && speed != 1 && decodeCtx->audioDecodeCtx->sonicStream) {
            sonicSetSpeed(decodeCtx->audioDecodeCtx->sonicStream, speed);
        }
        return true;
    } else {
        return false;
    }
}

EncodeNakedStream::EncodeNakedStream() {}

EncodeNakedStream::~EncodeNakedStream() {
    closeStream();
}