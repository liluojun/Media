//
// Created by hjt on 2025/6/25.
//

#ifndef MEDIA_AVSYNCCLOCK_H
#define MEDIA_AVSYNCCLOCK_H


#include <mutex>
#include <chrono>
#include <algorithm>
#include "../../common/include/native_log.h"
// AVSyncClock.h

class AVSyncClock {
public:
    void init(int64_t videoFirstPtsUs);

    void setPlaybackSpeed(double speed);
    double getPlaybackSpeed() const;

    int64_t getVideoPlayTimeUs();
    bool shouldRenderVideo(int64_t framePtsUs);
    int64_t syncAudio(int64_t audioPtsUs);

    // 返回播放速率因子：1.0 = 正常播放，<1.0 加速，>1.0 减速（音频可变速播放场景使用）
    double adjustPlaybackSpeed(int64_t audioPtsUs);

    void setThresholds(int64_t sleepThresholdUs, int64_t dropThresholdUs);
    int64_t getSleepThresholdUs() const;
    int64_t getDropThresholdUs() const;

private:
    mutable std::mutex mtx;
    int64_t baseVideoPtsUs = 0;
    std::chrono::high_resolution_clock::time_point baseSysTime;
    bool initialized = false;

    double playbackSpeed = 1.0; // 倍速播放因子：0.25x ~ 4x

    // 可调阈值
    int64_t sleepThresholdUs = 300000;  // 默认300ms
    int64_t dropThresholdUs = -150000;  // 默认-150ms
};


#endif //MEDIA_AVSYNCCLOCK_H
