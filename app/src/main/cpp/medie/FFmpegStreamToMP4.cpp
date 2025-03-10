//
// Created by hjt on 2025/3/8.
//

#include "FFmpegStreamToMP4.h"


#define SEI_UUID "6d1d9b05-42d5-40e3-ace3-f5d9b9f5b5b5"
#define CLEANUP() \
    do { \
        av_dict_free(&input_options); \
        avformat_close_input(&input_context); \
        if (output_context) { \
            if (!(output_context->oformat->flags & AVFMT_NOFILE)) \
                avio_closep(&output_context->pb); \
            avformat_free_context(output_context); \
        } \
        av_freep(&streams); \
        swr_free(&(audio_ctx.swr_ctx)); \
        avcodec_free_context(&(audio_ctx.dec_ctx)); \
        avcodec_free_context(&(audio_ctx.enc_ctx)); \
        av_frame_free(&(audio_ctx.frame)); \
        av_packet_free(&packet); \
    } while(0)

struct AudioContext {
    SwrContext *swr_ctx = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    AVCodecContext *enc_ctx = nullptr;
    AVFrame *frame = nullptr;
    int output_stream_idx = -1;
};

void ParseSEI(const AVPacket *pkt, std::vector<uint8_t> &sei_buffer) {
    const uint8_t *data = pkt->data;
    const uint8_t *end = pkt->data + pkt->size;

    while (data < end) {
        uint32_t nal_size = 0;

        // Annex B格式判断
        if (end - data >= 3 && data[0] == 0 && data[1] == 0 && data[2] == 1) {
            data += 3;
            nal_size = (data[0] & 0x1F);
            data += 1;
        } else if (end - data >= 4) {
            nal_size = AV_RB32(data);
            data += 4;
        } else {
            break;
        }

        if (data + nal_size > end) break;

        const uint8_t nal_type = data[0] & 0x1F;
        if (nal_type == 6 || nal_type == 39) { // SEI类型
            sei_buffer.insert(sei_buffer.end(), data, data + nal_size);
        }
        data += nal_size;
    }
}

void
FFmpegStreamToMP4::streamToMP4(const char *input_path, const char *output_path, jobject listener) {
// 初始化变量
    AVDictionary *input_options = nullptr;
    AVFormatContext *input_context = nullptr;
    AVFormatContext *output_context = nullptr;
    int *streams = nullptr;
    AVPacket *packet = av_packet_alloc();
    std::vector<uint8_t> sei_data;
    AudioContext audio_ctx;
    int ret = 0;
    int video_stream_idx = -1;
    int64_t video_duration = 0;

    // 设置输入选项
    av_dict_set(&input_options, "protocol_whitelist", "file,http,https,tcp,tls", 0);
    av_dict_set(&input_options, "fflags", "+discardcorrupt", 0);

    // 打开输入文件
    if ((ret = avformat_open_input(&input_context, input_path, nullptr, &input_options))) {
        LOGE("Open input failed: %s", av_err2str(ret));
        CallOnTransformFailed(listener, -2);
        CLEANUP();
        return;
    }

    // 获取流信息
    if ((ret = avformat_find_stream_info(input_context, nullptr))) {
        LOGE("Find stream info failed: %s", av_err2str(ret));
        CallOnTransformFailed(listener, -3);
        CLEANUP();
        return;
    }

    // 创建输出上下文
    if ((ret = avformat_alloc_output_context2(&output_context, nullptr, "mp4", output_path))) {
        LOGE("Create output failed: %s", av_err2str(ret));
        CallOnTransformFailed(listener, -4);
        CLEANUP();
        return;
    }

    // 流映射处理
    streams = static_cast<int *>(av_mallocz_array(input_context->nb_streams, sizeof(int)));
    for (int i = 0; i < input_context->nb_streams; ++i) {
        AVStream *in_stream = input_context->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;

        // 跳过非音视频流
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
            streams[i] = -1;
            continue;
        }

        // 视频流处理
        if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            if (in_codecpar->width <= 0 || in_codecpar->height <= 0) {
                LOGE("Invalid video dimensions");
                CallOnTransformFailed(listener, -5);
                CLEANUP();
                return;
            }
            video_duration = av_rescale_q(in_stream->duration,
                                          in_stream->time_base,
                                          AV_TIME_BASE_Q) / 1000;
        }

        // 音频流初始化
        if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
            (in_codecpar->codec_id == AV_CODEC_ID_PCM_MULAW ||
             in_codecpar->codec_id == AV_CODEC_ID_PCM_ALAW)) {

            // 初始化解码器
            const AVCodec *decoder = avcodec_find_decoder(in_codecpar->codec_id);
            audio_ctx.dec_ctx = avcodec_alloc_context3(decoder);
            avcodec_parameters_to_context(audio_ctx.dec_ctx, in_codecpar);
            if ((ret = avcodec_open2(audio_ctx.dec_ctx, decoder, nullptr))) {
                LOGE("Open audio decoder failed");
                CallOnTransformFailed(listener, -6);
                CLEANUP();
                return;
            }

            // 初始化编码器
            const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
            audio_ctx.enc_ctx = avcodec_alloc_context3(encoder);
            audio_ctx.enc_ctx->sample_rate = 44100;
            audio_ctx.enc_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
            audio_ctx.enc_ctx->channels = 2;
            audio_ctx.enc_ctx->sample_fmt = encoder->sample_fmts[0];
            audio_ctx.enc_ctx->bit_rate = 128000;
            if ((ret = avcodec_open2(audio_ctx.enc_ctx, encoder, nullptr))) {
                LOGE("Open audio encoder failed");
                CallOnTransformFailed(listener, -7);
                CLEANUP();
                return;
            }

            // 创建输出音频流
            AVStream *out_stream = avformat_new_stream(output_context, nullptr);
            out_stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
            avcodec_parameters_from_context(out_stream->codecpar, audio_ctx.enc_ctx);
            audio_ctx.output_stream_idx = out_stream->index;

            // 初始化重采样
            audio_ctx.swr_ctx = swr_alloc_set_opts(nullptr,
                                                   audio_ctx.enc_ctx->channel_layout,
                                                   audio_ctx.enc_ctx->sample_fmt,
                                                   audio_ctx.enc_ctx->sample_rate,
                                                   audio_ctx.dec_ctx->channel_layout,
                                                   audio_ctx.dec_ctx->sample_fmt,
                                                   audio_ctx.dec_ctx->sample_rate,
                                                   0, nullptr);
            swr_init(audio_ctx.swr_ctx);
        }

        // 创建输出流
        AVStream *out_stream = avformat_new_stream(output_context, nullptr);
        streams[i] = out_stream->index;
        avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
    }

    // 打开输出文件
    if (!(output_context->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&output_context->pb, output_path, AVIO_FLAG_WRITE))) {
            LOGE("Open output failed: %s", av_err2str(ret));
            CallOnTransformFailed(listener, -8);
            CLEANUP();
            return;
        }
    }

    // 写入文件头
    if ((ret = avformat_write_header(output_context, nullptr))) {
        LOGE("Write header failed: %s", av_err2str(ret));
        CallOnTransformFailed(listener, -9);
        CLEANUP();
        return;
    }

    // 处理数据包
    int64_t base_pts = AV_NOPTS_VALUE;
    while ((ret = av_read_frame(input_context, packet)) >= 0) {
        AVStream *in_stream = input_context->streams[packet->stream_index];
        const int stream_idx = packet->stream_index;

        // 视频流处理
        if (stream_idx == video_stream_idx) {
            ParseSEI(packet, sei_data);

            // 时间戳重置
            if (base_pts == AV_NOPTS_VALUE) {
                base_pts = packet->pts;
            }
            packet->pts -= base_pts;
            packet->dts -= base_pts;
        }
            // 音频流转码
        else if (audio_ctx.dec_ctx && stream_idx == audio_ctx.dec_ctx->time_base.den) {
            // 解码
            if ((ret = avcodec_send_packet(audio_ctx.dec_ctx, packet))) {
                LOGE("Send packet error: %s", av_err2str(ret));
                continue;
            }

            while (ret >= 0) {
                AVFrame *frame = av_frame_alloc();
                ret = avcodec_receive_frame(audio_ctx.dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

                // 重采样
                uint8_t *converted_data = nullptr;
                swr_convert(audio_ctx.swr_ctx, &converted_data, frame->nb_samples,
                            (const uint8_t **) frame->data, frame->nb_samples);

                // 编码
                AVPacket *aac_pkt = av_packet_alloc();
                if ((ret = avcodec_send_frame(audio_ctx.enc_ctx, frame))) {
                    av_frame_free(&frame);
                    break;
                }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(audio_ctx.enc_ctx, aac_pkt);
                    if (ret == AVERROR(EAGAIN)) continue;

                    // 时间戳处理
                    aac_pkt->stream_index = audio_ctx.output_stream_idx;
                    av_packet_rescale_ts(aac_pkt,
                                         audio_ctx.enc_ctx->time_base,
                                         output_context->streams[audio_ctx.output_stream_idx]->time_base);

                    // 写入输出
                    if ((ret = av_interleaved_write_frame(output_context, aac_pkt))) {
                        LOGE("Write audio failed: %s", av_err2str(ret));
                    }
                    av_packet_unref(aac_pkt);
                }
                av_frame_free(&frame);
            }
            av_packet_unref(packet);
            continue;
        }

        // 普通流处理
        av_packet_rescale_ts(packet,
                             in_stream->time_base,
                             output_context->streams[streams[stream_idx]]->time_base);
        if ((ret = av_interleaved_write_frame(output_context, packet))) {
            LOGE("Write frame failed: %s", av_err2str(ret));
        }
        av_packet_unref(packet);
    }

    // 写入SEI自定义Box
    if (!sei_data.empty()) {
        avio_write(output_context->pb, (const uint8_t *) "uuid", 4);
        avio_wb32(output_context->pb, 16 + sei_data.size());
        avio_write(output_context->pb, (const uint8_t *) SEI_UUID, 16);
        avio_write(output_context->pb, sei_data.data(), sei_data.size());
    }

    // 收尾处理
    av_write_trailer(output_context);
    CallOnTransformFinished(listener);
    CLEANUP();
}

void FFmpegStreamToMP4::CallOnTransformFailed(jobject listener, int err) {

}

void FFmpegStreamToMP4::CallOnTransformProgress(jobject listener, float progress) {

}

void FFmpegStreamToMP4::CallOnTransformFinished(jobject listener) {

}
