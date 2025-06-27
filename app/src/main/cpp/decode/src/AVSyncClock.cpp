
//
// Created by hjt on 2025/6/25.
//
// AVSyncClock.cpp
#include "../include/AVSyncClock.h"

/**
 * 初始化同步时钟
 *
 * 此函数用于初始化同步时钟，设置视频的初始PTS和当前系统时间
 * 它通过获取当前高分辨率时钟时间来设置baseSysTime，并将给定的videoFirstPtsUs作为视频的初始PTS
 * 初始化完成后，将initialized标志设置为true
 *
 * @param videoFirstPtsUs 视频的初始PTS（呈现时间戳），以微秒为单位
 */
void AVSyncClock::init(int64_t videoFirstPtsUs) {
    std::lock_guard<std::mutex> lock(mtx);
    baseVideoPtsUs = videoFirstPtsUs;
    baseSysTime = std::chrono::high_resolution_clock::now();
    initialized = true;
}

/**
 * 设置播放速度
 *
 * 此函数用于设置视频的播放速度播放速度被限制在0.25到4.0之间，以确保合理的播放范围
 *
 * @param speed 新的播放速度
 */
void AVSyncClock::setPlaybackSpeed(double speed) {
    std::lock_guard<std::mutex> lock(mtx);
    playbackSpeed = std::clamp(speed, 0.25, 4.0);
}

/**
 * 获取播放速度
 *
 * 此函数用于获取当前的播放速度
 *
 * @return 当前的播放速度
 */
double AVSyncClock::getPlaybackSpeed() const {
    std::lock_guard<std::mutex> lock(mtx);
    return playbackSpeed;
}

/**
 * 获取视频播放时间（单位：微秒）
 *
 * 此函数用于计算当前视频的播放时间它通过考虑视频的初始播放时间、当前系统时间和播放速度来完成计算
 * 函数首先检查同步时钟是否已初始化如果没有初始化，返回0表示没有播放时间
 * 然后，获取当前系统时间，并计算自视频开始播放以来经过的实时时间（微秒）
 * 接着，根据当前的播放速度计算逻辑播放时间这使得在不同播放速度下，能够正确地反映视频应播放到的时间点
 * 最后，将这个逻辑播放时间与视频的初始PTS（呈现时间戳）相加，得到当前的视频播放时间
 *
 * 注意：此函数在计算时使用了互斥锁，以确保在多线程环境下的线程安全
 *
 * @return 当前视频的播放时间（微秒），如果未初始化，则返回0
 */
int64_t AVSyncClock::getVideoPlayTimeUs() {
    // 确保线程安全
    std::lock_guard<std::mutex> lock(mtx);
    // 检查是否已初始化
    if (!initialized) return 0;
    // 获取当前系统时间
    auto now = std::chrono::high_resolution_clock::now();
    // 计算自视频开始播放以来经过的实时时间（微秒）
    auto elapsedRealTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(now - baseSysTime).count();

    // 计算逻辑播放时间（受 speed 影响）
    int64_t logicTimeUs = static_cast<int64_t>(elapsedRealTimeUs * playbackSpeed);

    // 返回当前的视频播放时间
    return baseVideoPtsUs + logicTimeUs;
}

/**
 * 判断是否应该渲染视频帧
 *
 * 此函数用于判断给定PTS的视频帧是否应该被渲染它通过比较帧的PTS和当前的视频播放时间来决定
 * 如果帧的PTS小于或等于当前的视频播放时间，则认为该帧应该被渲染
 *
 * @param framePtsUs 待判断的视频帧的PTS（呈现时间戳），以微秒为单位
 * @return 如果视频帧应该被渲染，则返回true；否则返回false
 */
bool AVSyncClock::shouldRenderVideo(int64_t framePtsUs) {
    int64_t nowPlayTimeUs = getVideoPlayTimeUs();
    int64_t delta = framePtsUs - nowPlayTimeUs;
    return delta <= 0;
}

/**
 * 同步音频PTS到视频播放时间
 *
 * 此函数用于计算音频PTS与当前视频播放时间之间的差异这有助于在音频和视频之间保持同步
 * 它通过从音频PTS中减去当前的视频播放时间来实现
 *
 * @param audioPtsUs 音频的PTS（呈现时间戳），以微秒为单位
 * @return 音频PTS与当前视频播放时间之间的差异，以微秒为单位
 */
int64_t AVSyncClock::syncAudio(int64_t audioPtsUs) {
    return audioPtsUs - getVideoPlayTimeUs();
}

/**
 * 根据音频PTS调整播放速度
 *
 * 此函数用于根据音频PTS与视频播放时间之间的差异来调整播放速度
 * 如果音频PTS超前太多（大于sleepThresholdUs），则减慢播放速度；如果音频PTS滞后太多（小于dropThresholdUs），则加快播放速度
 * 通过这种方式，可以动态调整播放速度，以保持音频和视频之间的同步
 *
 * @param audioPtsUs 音频的PTS（呈现时间戳），以微秒为单位
 * @return 调整后的播放速度
 */
double AVSyncClock::adjustPlaybackSpeed(int64_t audioPtsUs) {
    int64_t deltaUs = syncAudio(audioPtsUs);
    std::lock_guard<std::mutex> lock(mtx);
    if (deltaUs > sleepThresholdUs) {
        return std::max(0.25, playbackSpeed * 0.95);
    } else if (deltaUs < dropThresholdUs) {
        return std::min(4.0, playbackSpeed * 1.05);
    } else {
        return playbackSpeed;
    }
}

/**
 * 设置同步阈值
 *
 * 此函数用于设置音频和视频同步的阈值这包括sleepThresholdUs和dropThresholdUs，它们分别定义了音频PTS可以超前或滞后视频播放时间的最大值
 *
 * @param sleepThresholdUs 音频PTS可以超前视频播放时间的最大值（微秒）
 * @param dropThresholdUs 音频PTS可以滞后视频播放时间的最大值（微秒）
 */
void AVSyncClock::setThresholds(int64_t sleepThresholdUs, int64_t dropThresholdUs) {
    std::lock_guard<std::mutex> lock(mtx);
    this->sleepThresholdUs = sleepThresholdUs;
    this->dropThresholdUs = dropThresholdUs;
}

/**
 * 获取sleep阈值
 *
 * 此函数用于获取当前设置的sleep阈值
 *
 * @return 当前的sleep阈值（微秒）
 */
int64_t AVSyncClock::getSleepThresholdUs() const {
    std::lock_guard<std::mutex> lock(mtx);
    return sleepThresholdUs;
}

/**
 * 获取drop阈值
 *
 * 此函数用于获取当前设置的drop阈值
 *
 * @return 当前的drop阈值（微秒）
 */
int64_t AVSyncClock::getDropThresholdUs() const {
    std::lock_guard<std::mutex> lock(mtx);
    return dropThresholdUs;
}
