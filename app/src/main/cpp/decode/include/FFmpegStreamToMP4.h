//
// Created by hjt on 2025/3/8.
//

#ifndef MEDIA_FFMPEGSTREAMTOMP4_H
#define MEDIA_FFMPEGSTREAMTOMP4_H

#include <jni.h>
#include <vector>
#include <string>

#ifdef __cplusplus

extern "C" {
#include "../../common/include/native_log.h"
#include "../ffmpeg/include/libavformat/avformat.h"
#include "../ffmpeg/include/libavcodec/avcodec.h"
#include "../ffmpeg/include/libswresample/swresample.h"
#include "../ffmpeg/include/libavutil/intreadwrite.h"


#endif

class FFmpegStreamToMP4 {
public:
    void streamToMP4(const char *input_path,
                     const char *output_path,
                     jobject listener);

    void CallOnTransformFailed(jobject listener, int err);

    void CallOnTransformProgress(jobject listener, float progress);

    void CallOnTransformFinished(jobject listener);
};

#ifdef __cplusplus
}
#endif
#endif //MEDIA_FFMPEGSTREAMTOMP4_H
