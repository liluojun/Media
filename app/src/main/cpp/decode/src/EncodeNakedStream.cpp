//
// Created by hjt on 2025/4/11.
//

#include "../include/EncodeNakedStream.h"

static enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
enum NalUnitTypeH264 {
    NALU_TYPE_SPS = 7,
    NALU_TYPE_PPS = 8,
};

enum NalUnitTypeH265 {
    NALU_TYPE_VPS = 32,
    NALU_TYPE_SPS_HEVC = 33,
    NALU_TYPE_PPS_HEVC = 34,
};
enum CodecType {
    CODEC_H264,
    CODEC_H265
};

bool extractAnnexBNalus(const uint8_t *data, size_t size,
                        std::vector<std::vector<uint8_t>> &nalus) {
    size_t i = 0;
    while (i + 4 < size) {
        // 查找 startcode
        if (data[i] == 0 && data[i + 1] == 0 &&
            data[i + 2] == 0 && data[i + 3] == 1) {
            size_t start = i + 4;
            i = start;
            while (i + 4 < size &&
                   !(data[i] == 0 && data[i + 1] == 0 &&
                     data[i + 2] == 0 && data[i + 3] == 1)) {
                ++i;
            }
            nalus.emplace_back(data + start, data + i);
        } else {
            ++i;
        }
    }
    return !nalus.empty();
}

bool buildExtradata(const std::vector<std::vector<uint8_t>> &nalus,
                    CodecType codecType,
                    std::vector<uint8_t> &out) {
    std::vector<uint8_t> sps, pps, vps;

    for (const auto &nalu: nalus) {
        if (nalu.empty()) continue;
        uint8_t nal_unit_type = (codecType == CODEC_H264)
                                ? (nalu[0] & 0x1F)     // H264
                                : ((nalu[0] >> 1) & 0x3F); // H265

        if (codecType == CODEC_H264) {
            if (nal_unit_type == 7) {
                sps = nalu;
            } else if (nal_unit_type == 8) {
                pps = nalu;
            }
        } else {
            if (nal_unit_type == 32) {
                vps = nalu;
            }
            else if (nal_unit_type == 33) {
                sps = nalu;
            }
            else if (nal_unit_type == 34) {
                pps = nalu;
            }
        }
    }

    if ((codecType == CODEC_H264 && (sps.empty() || pps.empty())) ||
        (codecType == CODEC_H265 && (vps.empty() || sps.empty() || pps.empty()))) {
        return false;
    }

    out.clear();

    if (codecType == CODEC_H264) {
        // 参考 FFmpeg H.264 AVCC 格式
        out.push_back(0x01);                 // configurationVersion
        out.push_back(sps[1]);               // profile
        out.push_back(sps[2]);               // profile compat
        out.push_back(sps[3]);               // level
        out.push_back(0xFF);                 // 6 bits reserved + 2 bits lengthSizeMinusOne
        out.push_back(0xE1);                 // 3 bits reserved + 5 bits numOfSPS

        out.push_back((sps.size() >> 8) & 0xFF);
        out.push_back(sps.size() & 0xFF);
        out.insert(out.end(), sps.begin(), sps.end());

        out.push_back(1); // numOfPPS
        out.push_back((pps.size() >> 8) & 0xFF);
        out.push_back(pps.size() & 0xFF);
        out.insert(out.end(), pps.begin(), pps.end());

    } else {
        // HVCC 构造（参考上一轮 HVCC 代码）
        out.push_back(0x01); // configurationVersion
        out.insert(out.end(), {
                sps[1], sps[2], sps[3], sps[4], sps[5], sps[6], sps[7], sps[8],
                sps[9], sps[10], sps[11], sps[12]
        });
        out.insert(out.end(), {
                0xF0, 0x00, 0xFC, 0xFD, 0xF8, 0xF8, 0x00, 0x00,
                0x0F, 0x03  // numOfArrays = 3
        });

        auto write_array = [&](uint8_t nal_unit_type, const std::vector<uint8_t> &nalu) {
            out.push_back((1 << 7) | nal_unit_type); // completeness + nal type
            out.push_back(0x00); // numNalus >> 8
            out.push_back(0x01); // numNalus
            out.push_back((nalu.size() >> 8) & 0xFF);
            out.push_back(nalu.size() & 0xFF);
            out.insert(out.end(), nalu.begin(), nalu.end());
        };

        write_array(32, vps);
        write_array(33, sps);
        write_array(34, pps);
    }

    return true;
}

bool extractAndConvertExtradata(const uint8_t *data, size_t size, AVCodecID codecId,
                                std::vector<uint8_t> &outExtradata) {
    CodecType codecType = (codecId == AV_CODEC_ID_H264) ? CODEC_H264 : CODEC_H265;

    std::vector<std::vector<uint8_t>> nalus;
    if (!extractAnnexBNalus(data, size, nalus)) {
        return false;
    }

    return buildExtradata(nalus, codecType, outExtradata);
}

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

void printExtradataHex_LOGE(const uint8_t *data, int size, int bytesPerLine = 16) {
    if (!data || size <= 0) {
        LOGE("extradata为空或大小无效");
        return;
    }
    char line[256] = {0};
    for (int i = 0; i < size; i++) {
        if (i % bytesPerLine == 0) {
            if (i != 0) LOGE("%s", line);
            snprintf(line, sizeof(line), "%04X: ", i);
        }
        int len = strlen(line);
        snprintf(line + len, sizeof(line) - len, "%02X ", data[i]);
    }
    if (size > 0) {
        LOGE("%s", line);
    }
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
            LOGE("NakedFrameData %d       %d", sizeof(data->data),data->size)
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
                LOGE("yingjie1")
                if (data->frametype != PktIFrames) {
                    LOGE("非I帧无法搜寻sps与pps")
                    return -5;
                } else {
                    std::vector<uint8_t> extradata;
                    if (extractAndConvertExtradata(data->data, data->size, data->codecId,
                                                   extradata)) {

                        ctx->videoDecodeCtx->videoCodecCtx->extradata_size = (int) extradata.size();
                        ctx->videoDecodeCtx->videoCodecCtx->extradata = (uint8_t *) av_mallocz(
                                extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE);
                        memcpy(ctx->videoDecodeCtx->videoCodecCtx->extradata, extradata.data(),
                               extradata.size());
                        LOGE("成功提取并设置 extradata，大小：%d",
                             ctx->videoDecodeCtx->videoCodecCtx->extradata_size);
                    } else {
                        LOGE("未能提取到有效的 SPS/PPS（或 VPS）");
                    }
                }
                for (int i = 0;; i++) {
                    const AVCodecHWConfig *config = avcodec_get_hw_config(videoCodec, i);
                    if (!config)
                        break;
                    if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                        config->device_type == AV_HWDEVICE_TYPE_MEDIACODEC) {
                        hw_pix_fmt = config->pix_fmt; // ✅ 初始化正确的格式
                        LOGE("hw_pix_fmt=%d", hw_pix_fmt);
                        break;
                    }
                }

                AVBufferRef *hw_device_ctx = nullptr;
                int result = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_MEDIACODEC,
                                                    nullptr, nullptr, 0);
                LOGE("create HW device context  result=%d", result);
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
                        LOGE("pix_fmts=%d", *pix_fmts);
                        if (*pix_fmts == hw_pix_fmt) {
                            LOGE("pix_fmts***=%d", *pix_fmts);
                            return *pix_fmts;
                        }
                    }
                    return AV_PIX_FMT_NONE;
                };
                ctx->videoDecodeCtx->videoCodecCtx->width = data->width;
                ctx->videoDecodeCtx->videoCodecCtx->height = data->height;

                LOGE("准备打开的解码器是: %s  %d", videoCodec->name, videoCodec->id);
                LOGE("extradata_size=%d", ctx->videoDecodeCtx->videoCodecCtx->extradata_size);
                printExtradataHex_LOGE(ctx->videoDecodeCtx->videoCodecCtx->extradata,
                                       ctx->videoDecodeCtx->videoCodecCtx->extradata_size);

                LOGE("width=%d height=%d", ctx->videoDecodeCtx->videoCodecCtx->width,
                     ctx->videoDecodeCtx->videoCodecCtx->height);
                int ret = avcodec_open2(ctx->videoDecodeCtx->videoCodecCtx, videoCodec, NULL);
                LOGE("ret=%d", ret)
                char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(ret, errbuf, sizeof(errbuf));
                LOGE("avcodec_open2 , ret=%d, reason=%s", ret, errbuf);
                if (ret < 0) {
                    avcodec_free_context(&ctx->videoDecodeCtx->videoCodecCtx);
                    ctx->videoInitFailClean();
                    return -4;
                }
                LOGE("yingjie2")
                LOGE("After open codec, pix_fmt=%d", ctx->videoDecodeCtx->videoCodecCtx->pix_fmt);
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
                    continue;
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