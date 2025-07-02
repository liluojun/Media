//
// Created by hjt on 2025/7/2.
//
#include "../include/DecodeCommonUtils.h"
AVFrame* DecodeCommonUtils::ConvertToYUV420PWithLibyuv(const AVFrame* src) {
    if (!src) return nullptr;

    // 只处理NV12或NV21格式
    if (src->format != AV_PIX_FMT_NV12 && src->format != AV_PIX_FMT_NV21) {
        return nullptr;
    }

    int width = src->width;
    int height = src->height;

    // 新建YUV420P帧
    AVFrame* dst = av_frame_alloc();
    if (!dst) return nullptr;

    dst->format = AV_PIX_FMT_YUV420P;
    dst->width = width;
    dst->height = height;

    if (av_frame_get_buffer(dst, 32) < 0) {
        av_frame_free(&dst);
        return nullptr;
    }

    // libyuv输入参数
    const uint8_t* src_y = src->data[0];
    int src_stride_y = src->linesize[0];
    const uint8_t* src_uv = src->data[1];
    int src_stride_uv = src->linesize[1];

    // libyuv输出参数
    uint8_t* dst_y = dst->data[0];
    int dst_stride_y = dst->linesize[0];
    uint8_t* dst_u = dst->data[1];
    int dst_stride_u = dst->linesize[1];
    uint8_t* dst_v = dst->data[2];
    int dst_stride_v = dst->linesize[2];

    bool is_nv21 = (src->format == AV_PIX_FMT_NV21);

    int ret = is_nv21 ?
              libyuv::NV21ToI420(src_y, src_stride_y,
                                 src_uv, src_stride_uv,
                                 dst_y, dst_stride_y,
                                 dst_u, dst_stride_u,
                                 dst_v, dst_stride_v,
                                 width, height) :
              libyuv::NV12ToI420(src_y, src_stride_y,
                                 src_uv, src_stride_uv,
                                 dst_y, dst_stride_y,
                                 dst_u, dst_stride_u,
                                 dst_v, dst_stride_v,
                                 width, height);

    if (ret != 0) {
        av_frame_free(&dst);
        return nullptr;
    }

    return dst;
}