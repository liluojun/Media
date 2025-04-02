//
// Created by hjt on 2025/4/2.
//

#include "AudioPlayer.h"
AudioPlayer::AudioPlayer() {}

AudioPlayer::~AudioPlayer() {
    stopPlayback();
}

bool AudioPlayer::startPlayback(int sampleRate, int channelCount, oboe::AudioFormat format) {
    oboe::AudioStreamBuilder builder;

    // 配置音频流参数
    builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(format)
            ->setSampleRate(sampleRate)
            ->setChannelCount(channelCount)
            ->setDataCallback(this);  // 设置回调对象

    // 打开音频流
    oboe::Result result = builder.openStream(&mPlaybackStream);
    if (result != oboe::Result::OK || !mPlaybackStream) {
        // 处理错误
        return false;
    }

    // 启动音频流
    result = mPlaybackStream->requestStart();
    return result == oboe::Result::OK;
}

void AudioPlayer::stopPlayback() {
    if (mPlaybackStream) {
        mPlaybackStream->stop();
        mPlaybackStream->close();
        mPlaybackStream = nullptr;
    }
}

// 外部写入 PCM 数据到缓冲区
void AudioPlayer::writeData(const void *data, size_t size) {
    std::lock_guard<std::mutex> lock(mDataMutex);
    const uint8_t *src = static_cast<const uint8_t*>(data);
    mPCMBuffer.insert(mPCMBuffer.end(), src, src + size);
}

// 音频回调：填充数据到音频设备
oboe::DataCallbackResult AudioPlayer::onAudioReady(
        oboe::AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {

    if (!audioStream || !audioData) {
        return oboe::DataCallbackResult::Stop;
    }

    size_t bytesPerFrame = audioStream->getBytesPerFrame();
    size_t requestSize = numFrames * bytesPerFrame;
    uint8_t *dst = static_cast<uint8_t*>(audioData);

    std::lock_guard<std::mutex> lock(mDataMutex);

    // 检查缓冲区是否有足够数据
    if (mPCMBuffer.size() >= requestSize) {
        // 拷贝数据到音频设备缓冲区
        memcpy(dst, mPCMBuffer.data(), requestSize);
        // 移除已播放的数据
        mPCMBuffer.erase(mPCMBuffer.begin(), mPCMBuffer.begin() + requestSize);
        return oboe::DataCallbackResult::Continue;
    } else {
        // 数据不足，填充静音（0）
        memset(dst, 0, requestSize);
        return oboe::DataCallbackResult::Continue;
    }
}
