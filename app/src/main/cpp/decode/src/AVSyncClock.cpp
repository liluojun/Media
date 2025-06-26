//
// Created by hjt on 2025/6/25.
//
// AVSyncClock.cpp
#include "../include/AVSyncClock.h"

void AVSyncClock::init(int64_t videoFirstPtsUs) {
    std::lock_guard<std::mutex> lock(mtx);
    baseVideoPtsUs = videoFirstPtsUs;
    baseSysTime = std::chrono::high_resolution_clock::now();
    initialized = true;
}

void AVSyncClock::setPlaybackSpeed(double speed) {
    std::lock_guard<std::mutex> lock(mtx);
    playbackSpeed = std::clamp(speed, 0.25, 4.0);
}

double AVSyncClock::getPlaybackSpeed() const {
    return playbackSpeed;
}

int64_t AVSyncClock::getVideoPlayTimeUs() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!initialized) return 0;
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - baseSysTime).count();
    return baseVideoPtsUs + static_cast<int64_t>(elapsed * playbackSpeed);
}

bool AVSyncClock::shouldRenderVideo(int64_t framePtsUs) {
    int64_t currentPlayTime = getVideoPlayTimeUs();
    return framePtsUs <= currentPlayTime;
}

int64_t AVSyncClock::syncAudio(int64_t audioPtsUs) {
    int64_t currentPlayTime = getVideoPlayTimeUs();
    return audioPtsUs - currentPlayTime;
}

double AVSyncClock::adjustPlaybackSpeed(int64_t audioPtsUs) {
    int64_t deltaUs = syncAudio(audioPtsUs);
    if (deltaUs > sleepThresholdUs) {
        return std::max(0.25, playbackSpeed * 0.95);
    } else if (deltaUs < dropThresholdUs) {
        return std::min(4.0, playbackSpeed * 1.05);
    } else {
        return playbackSpeed;
    }
}

void AVSyncClock::setThresholds(int64_t sleepThresholdUs, int64_t dropThresholdUs) {
    std::lock_guard<std::mutex> lock(mtx);
    this->sleepThresholdUs = sleepThresholdUs;
    this->dropThresholdUs = dropThresholdUs;
}

int64_t AVSyncClock::getSleepThresholdUs() const {
    return sleepThresholdUs;
}

int64_t AVSyncClock::getDropThresholdUs() const {
    return dropThresholdUs;
}
