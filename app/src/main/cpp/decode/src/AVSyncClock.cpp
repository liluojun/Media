//
// Created by hjt on 2025/6/25.
//

#include "../include/AVSyncClock.h"
void AVSyncClock::init(int64_t videoFirstPtsUs) {
    std::lock_guard<std::mutex> lock(mtx);
    baseVideoPtsUs = videoFirstPtsUs;
    baseSysTime = std::chrono::high_resolution_clock::now();
    initialized = true;
}

int64_t AVSyncClock::getVideoPlayTimeUs() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!initialized) return 0;
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - baseSysTime).count();
    return baseVideoPtsUs + elapsed;
}

bool AVSyncClock::shouldRenderVideo(int64_t framePtsUs) {
    int64_t currentPlayTime = getVideoPlayTimeUs();
    return framePtsUs <= currentPlayTime;
}

int64_t AVSyncClock::syncAudio(int64_t audioPtsUs) {
    int64_t currentPlayTime = getVideoPlayTimeUs();
    return audioPtsUs - currentPlayTime;
}