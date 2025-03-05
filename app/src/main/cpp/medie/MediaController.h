//
// Created by hjt on 2025/2/26.
//

#ifndef MEDIA_MEDIACONTROLLER_H
#define MEDIA_MEDIACONTROLLER_H

#include "FFmpegEncodeStream.h"
#include "native_log.h"
#include "GlThread.h"
#include <cstring>
#include <map>
#include "FrameCallback.h"
#include "Utils.cpp"

#ifdef __cplusplus
/**
 * @brief 播放器核心上下文结构体
 *
 * 管理编码流对象、OpenGL渲染线程及Surface创建状态，
 * 包含编码回调代理类的实现和回调设置逻辑
 */
typedef struct {
    FFmpegEncodeStream *mFFmpegEncodeStream = NULL;
    GlThread *mGlThread = NULL;


    /**
     * @brief 编码回调代理类
     *
     * 继承自帧回调接口，实现将编码完成的视频帧传递给渲染线程
     */
    struct EncodeCallback : public FrameCallback {
        GlThread *glThread;  ///< 关联的OpenGL渲染线程对象

        /**
         * @brief 构造函数
         * @param thread 需要关联的OpenGL渲染线程实例
         */
        explicit EncodeCallback(GlThread *thread) : glThread(thread) {}

        /**
         * @brief 帧编码完成回调函数
         * @param mAVFrame 已编码的帧数据对象
         *
         * 重写自基类FrameCallback，当视频帧完成硬件编码时被调用，
         * 负责将编码后的帧数据传递给渲染线程进行后续处理
         */
        void onFrameEncoded(AVFrame *mAVFrame) override {
            if (glThread && glThread->getIsSurfaceCreated() && mAVFrame) {
                // 创建YUV数据对象
                YuvData *yuvData = new YuvData();

                // 获取YUV分量参数
                const int y_width = mAVFrame->width;
                const int y_height = mAVFrame->height;
                const int uv_width = y_width / 2;
                const int uv_height = y_height / 2;

                // 分配内存并复制Y分量
                yuvData->y = new uint8_t[y_width * y_height];
                for (int i = 0; i < y_height; ++i) {
                    memcpy(yuvData->y + i * y_width,
                           mAVFrame->data[0] + i * mAVFrame->linesize[0],
                           y_width);
                }

                // 复制U分量（Cb）
                yuvData->u = new uint8_t[uv_width * uv_height];
                for (int i = 0; i < uv_height; ++i) {
                    memcpy(yuvData->u + i * uv_width,
                           mAVFrame->data[1] + i * mAVFrame->linesize[1],
                           uv_width);
                }

                // 复制V分量（Cr）
                yuvData->v = new uint8_t[uv_width * uv_height];
                for (int i = 0; i < uv_height; ++i) {
                    memcpy(yuvData->v + i * uv_width,
                           mAVFrame->data[2] + i * mAVFrame->linesize[2],
                           uv_width);
                }
// 处理可能存在的行对齐填充
                const int y_stride = mAVFrame->linesize[0];
                if (y_stride == y_width) { // 无填充情况
                    memcpy(yuvData->y, mAVFrame->data[0], y_width * y_height);
                } else { // 存在行填充
                    for (int i = 0; i < y_height; ++i) {
                        memcpy(yuvData->y + i * y_width,
                               mAVFrame->data[0] + i * y_stride,
                               y_width);
                    }
                }
                // 传递给渲染线程
                glThread->postMessage(kMsgYuvData, y_width, y_height, yuvData);
            }
        }
    };

    /**
     * @brief 初始化编码回调
     *
     * 当编码流对象和渲染线程都就绪时，创建静态回调实例，
     * 将编码流与回调代理对象进行绑定
     */
    void setupCallback() {
        if (mFFmpegEncodeStream && mGlThread) {
            static EncodeCallback cb(mGlThread);
            mFFmpegEncodeStream->setFrameCallback(&cb);
        }
    }
} Player;

extern "C" {
#include <string>

#endif
class MediaController {
public:
    int openStream(std::string *path);

    int creatSurface(std::string *path, ANativeWindow *mWindow, int w, int h);

    int destorySurface(std::string *path);

    int changeSurfaceSize(std::string *path, int w, int h);

    int closeStream(std::string *path);


    ~MediaController();

    // 新增 map 声明 (路径字符串 -> player 对象指针)
    std::map<std::string, Player *> pathPlayerMap;

};
#ifdef __cplusplus
}
#endif

#endif //MEDIA_MEDIACONTROLLER_H
