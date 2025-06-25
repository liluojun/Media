//
// Created by hjt on 2025/6/25.
//

#ifndef MEDIA_AVSYNCCLOCK_H
#define MEDIA_AVSYNCCLOCK_H


#include <mutex>
#include <chrono>

class AVSyncClock {
public:
    void init(int64_t videoFirstPtsUs);  // 初始化同步基准
    int64_t getVideoPlayTimeUs();        // 获取当前应该播放的 video pts（系统时间映射）
    bool shouldRenderVideo(int64_t framePtsUs); // 当前 video 帧是否可播放
    int64_t syncAudio(int64_t audioPtsUs);      // 返回audio相对video偏移：<0落后 >0超前

private:
    std::mutex mtx;
    int64_t baseVideoPtsUs = 0;
    std::chrono::high_resolution_clock::time_point baseSysTime;
    bool initialized = false;
};


#endif //MEDIA_AVSYNCCLOCK_H
