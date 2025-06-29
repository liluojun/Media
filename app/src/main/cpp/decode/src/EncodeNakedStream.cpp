//
// Created by hjt on 2025/4/11.
//

#include "../include/EncodeNakedStream.h"



static enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;

int get_nal_type(const uint8_t *data, int codecId) {
    if (codecId == AV_CODEC_ID_H264) {
        return data[0] & 0x1F;  // H264: lower 5 bits
    } else if (codecId == AV_CODEC_ID_HEVC) {
        return (data[0] >> 1) & 0x3F;  // H265: bits 1-6
    }
    return -1;
}
// 将一帧 H264 AnnexB 格式转为 AVCC 格式（用于 MediaCodec 解码）
std::vector<uint8_t> convertAnnexBToAVCC(const uint8_t* data, size_t size) {
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
       /* LOGE("data->frametype=%d    data->size=%d    data->pts=%d  width=%d,  height=%d", data->frametype, data->size,
             data->pts,data->width,data->height)*/
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
 void buildAvccExtradata(const std::vector<uint8_t>& sps,
                                        const std::vector<uint8_t>& pps,std::vector<uint8_t>& extradata) {


    if (sps.size() < 4) return ;

    // AVCC header
    extradata.push_back(0x01);               // configurationVersion
    extradata.push_back(sps[1]);             // profile_idc
    extradata.push_back(sps[2]);             // profile_compat
    extradata.push_back(sps[3]);             // level_idc
    extradata.push_back(0xFF);               // 6 bits reserved + 2 bits lengthSizeMinusOne (4 bytes)

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

    return ;
}

int initVideoAVCodec(InitContext *ctx, const AVCodec *videoCodec, NakedFrameData *data,
                     AVCodecID *lastAVCodecID) {
    if (videoCodec != nullptr || (*lastAVCodecID == AV_CODEC_ID_NONE) ||
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
            LOGE("NakedFrameData %d       %d", sizeof(data->data), data->size)
            char codecName[64] = {0};
            getAndroidCodec(data->codecId, codecName);
            if (strlen(codecName) > 0) {
                LOGE("yingjie")
                videoCodec = avcodec_find_decoder_by_name(codecName);
            }
            if (!videoCodec) {
//                LOGE("ruanjie")
//                videoCodec = avcodec_find_decoder(data->codecId);
//                if (!videoCodec) {
//                    LOGE("Video codec not found");
//                    ctx->videoInitFailClean();
//                    return -2;
//                }
//                if (!(ctx->videoDecodeCtx->videoCodecCtx = avcodec_alloc_context3(videoCodec))) {
//                    LOGE("Failed to allocate video codec context");
//                    ctx->videoInitFailClean();
//                    return -3;
//                }
//                ctx->videoDecodeCtx->videoCodecCtx->width = data->width;
//                ctx->videoDecodeCtx->videoCodecCtx->height = data->height;
//                ctx->videoDecodeCtx->videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
//                ctx->videoDecodeCtx->user_time_base = (AVRational) {1, 25};
//                ctx->videoDecodeCtx->frame_count = 0;
//                if (avcodec_open2(ctx->videoDecodeCtx->videoCodecCtx, videoCodec, NULL) < 0) {
//                    avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
//                    ctx->videoInitFailClean();
//                    return -4;
//                }
            } else {
                ctx->videoDecodeCtx->videoCodecCtx = avcodec_alloc_context3(videoCodec);
                if (data->frametype != PktIFrames) {
                    LOGE("非I帧无法搜寻sps与pps")
                    return -5;
                } else {
                    CodecExtraData extra = parseSpsPpsVpsFromIFrame(data->data, data->size,
                                                                    data->codecId);
                    std::vector<uint8_t> extradata;
                    auto appendNalWithLengthPrefix = [](std::vector<uint8_t>& dest, const std::vector<uint8_t>& nal) {
                        uint32_t len = nal.size();
                        dest.push_back((len >> 24) & 0xFF);
                        dest.push_back((len >> 16) & 0xFF);
                        dest.push_back((len >> 8) & 0xFF);
                        dest.push_back(len & 0xFF);
                        dest.insert(dest.end(), nal.begin(), nal.end());
                    };
                    if (data->codecId == AV_CODEC_ID_H264) {
                        buildAvccExtradata(extra.sps,extra.pps,extradata);
                        LOGE("SPS size=%zu, first bytes: ", extra.sps.size());
                        for (int i = 0; i < std::min((size_t)16, extra.sps.size()); ++i)
                            LOGE("%02X ", extra.sps[i]);


                        LOGE("PPS size=%zu, first bytes: ", extra.pps.size());
                        for (int i = 0; i < std::min((size_t)16, extra.pps.size()); ++i)
                            LOGE("%02X ", extra.pps[i]);

//                        if (!extra.sps.empty()) appendNalWithLengthPrefix(extradata, extra.sps);
//                        if (!extra.pps.empty()) appendNalWithLengthPrefix(extradata, extra.pps);
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
                    LOGE("extradata size=%d, first 16 bytes:", ctx->videoDecodeCtx->videoCodecCtx->extradata_size);
                    for (int i = 0; i < 16 && i < extradata.size(); i++) {
                        LOGE("%02X ", extradata[i]);
                    }

                }

                for (int i = 0;; i++) {
                    const AVCodecHWConfig *config = avcodec_get_hw_config(videoCodec, i);
                    if (!config)
                        break;
                    const char* name = av_get_pix_fmt_name(config->pix_fmt);
                    LOGE("pix_fmt=%d name=%s\n", config->pix_fmt, name);
                    if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                        config->device_type == AV_HWDEVICE_TYPE_MEDIACODEC) {
                        hw_pix_fmt = config->pix_fmt; // ✅ 初始化正确的格式
                        break;
                    }
                }
                AVBufferRef *hw_device_ctx = nullptr;
                enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
                while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
                    LOGE("Supported HW device: %s", av_hwdevice_get_type_name(type));
                }
                int result = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_MEDIACODEC,
                                                    nullptr, nullptr, 0);
                if (result < 0) {
                    LOGE("Failed to create HW device context");
                    return -11;
                }
                ctx->videoDecodeCtx->videoCodecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
                av_buffer_unref(&hw_device_ctx); // ✅ 增加这一行释放临时引用
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
                ctx->videoDecodeCtx->videoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
                ctx->videoDecodeCtx->videoCodecCtx->width = data->width;
                ctx->videoDecodeCtx->videoCodecCtx->height = data->height;
                ctx->videoDecodeCtx->videoCodecCtx->pix_fmt=hw_pix_fmt;
                ctx->videoDecodeCtx->videoCodecCtx->sw_pix_fmt = AV_PIX_FMT_NV12;
                LOGE("open codec, pix_fmt=%d   w=%d, h=%d", ctx->videoDecodeCtx->videoCodecCtx->pix_fmt,ctx->videoDecodeCtx->videoCodecCtx->width,ctx->videoDecodeCtx->videoCodecCtx->height);
                int ret = avcodec_open2(ctx->videoDecodeCtx->videoCodecCtx, videoCodec, NULL);
                char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                LOGE("avcodec_open2 , ret=%d, reason=%s", ret, errbuf);
                if (ret < 0) {
                    avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
                    ctx->videoInitFailClean();
                    return -4;
                }
                LOGE("After open codec, pix_fmt=%d", ctx->videoDecodeCtx->videoCodecCtx->pix_fmt);
                if (ctx->videoDecodeCtx->videoCodecCtx->hw_device_ctx) {
                    AVHWDeviceContext *hw_ctx = (AVHWDeviceContext*)ctx->videoDecodeCtx->videoCodecCtx->hw_device_ctx->data;
                    LOGE("Active HW device: %s", av_hwdevice_get_type_name(hw_ctx->type));

                    if (ctx->videoDecodeCtx->videoCodecCtx->hw_frames_ctx) {
                        AVHWFramesContext *frame_ctx = (AVHWFramesContext*)ctx->videoDecodeCtx->videoCodecCtx->hw_frames_ctx->data;
                        LOGE("HW frame format: %s", av_get_pix_fmt_name(frame_ctx->format));
                    }
                } else {
                    LOGE("HW device context NOT set!");
                }

// 获取详细错误信息
                const char *err_desc = av_err2str(ret);
                LOGE("avcodec_open2 error: %s", err_desc);

// 检查 MediaCodec 私有错误
                if (ctx->videoDecodeCtx->videoCodecCtx->priv_data) {
                    char *mediacodec_err = NULL;
                    av_opt_get(ctx->videoDecodeCtx->videoCodecCtx->priv_data, "error_detail", 0, (uint8_t**)&mediacodec_err);
                    if (mediacodec_err) {
                        LOGE("MediaCodec internal error: %s", mediacodec_err);
                    }
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
}

void videoThread(InitContext *ctx) {
    const AVCodec *videoCodec = nullptr;
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
        std::vector<uint8_t> avccFrame = convertAnnexBToAVCC(packet->data, packet->size);
        av_new_packet(pkt, avccFrame.size());
        memcpy(pkt->data, avccFrame.data(), avccFrame.size());
        LOGE("NALU converted size=%zu, first 16 bytes:", avccFrame.size());
        for (int i = 0; i < 16 && i < avccFrame.size(); i++) {
            LOGE("%02X ", avccFrame[i]);
        }
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
            LOGE("frame->format=%d ", frame->format);
            AVFrame *finalFrame = frame;
            // 如果是硬解帧，进行内存拷贝到系统内存（sw_frame）
            if (frame->format == hw_pix_fmt) {
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
                    return;
                    //continue;
                }

                finalFrame = sw_frame;  // 用 sw_frame 代替原 frame（即数据在系统内存）
                av_frame_unref(frame);  // 原始硬件帧可以释放
                if (finalFrame != frame) {
                    av_frame_free(&frame);  // 真正释放原始 frame
                }
            }
            AVFrame *frameCopy = av_frame_clone(finalFrame);

            pthread_mutex_lock(&ctx->videoDecodeCtx->viedoDecodeMutex);
            while (ctx->videoDecodeCtx->videoDecodeQueue.size() >=
                   ctx->videoDecodeCtx->MAX_VIDEO_FRAME &&
                   !ctx->videoDecodeCtx->abortRequest) {
                pthread_cond_wait(&ctx->videoDecodeCtx->viedoDecodeFullCond,
                                  &ctx->videoDecodeCtx->viedoDecodeMutex);
            }
            ctx->videoDecodeCtx->videoDecodeQueue.push(frameCopy);
            LOGE("完成添加******");
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
        // LOGE("视频帧PTS=%lld,系统时间=%lld,时间差=%lld   queue=%d", framePtsUs, ctx->syncClock->getVideoPlayTimeUs(),deltaUs,ctx->videoDecodeCtx->videoDecodeQueue.size())
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
                    // ctx->audioReadDecode.push(data);
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