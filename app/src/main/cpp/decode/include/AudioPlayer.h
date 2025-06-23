//
// Created by hjt on 2025/4/2.
//

#ifndef MEDIA_AUDIOPLAYER_H
#define MEDIA_AUDIOPLAYER_H
#include "oboe/Oboe.h"
#include "../../common/include/native_log.h"
#include <iostream>
#include <fstream>
#include <vector>

class AudioPlayer : public oboe::AudioStreamDataCallback {
public:
    AudioPlayer();
    ~AudioPlayer();

    // 初始化音频流
    bool startPlayback(int sampleRate, int channelCount, oboe::AudioFormat format);
    void stopPlayback();

    // 写入 PCM 数据（需自行管理数据缓冲）
    void writeData(const void *data, size_t size);

    // 音频流回调接口
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int32_t numFrames) override;

private:
    std::mutex mDataMutex;
    std::vector<uint8_t> mPCMBuffer;  // 环形缓冲区存储 PCM 数据
    oboe::AudioStream *mPlaybackStream = nullptr;

};


#endif //MEDIA_AUDIOPLAYER_H
