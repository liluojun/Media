//
// Created by hjt on 2025/2/26.
//
#include "MediaController.h"

int MediaController::openStream(std::string *path) {
    int result = 0;
    auto it = pathPlayerMap.find(*path);
    if (it == pathPlayerMap.end()) {
        Player *player = new Player();
        player->mFFmpegEncodeStream = new FFmpegEncodeStream();

        player->mGlThread = new GlThread();
        pathPlayerMap[*path] = player;
        player->setupCallback();
        player->mFFmpegEncodeStream->openStream(stringToChar(*path).data());
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s already exists", path);
    }
    return result;
}

int MediaController::creatSurface(std::string *path, ANativeWindow *mWindow, int w, int h) {
    int result = 0;
    auto it = pathPlayerMap.find(*path);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            it->second->mGlThread->postMessage(kMsgSurfaceCreated, w, h, mWindow);
        } else {
            result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
            LOGE("Player is null on creatSurface  path %s", path);
        }
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s not exists on creatSurface", path);
    }
    return result;
}

int MediaController::destorySurface(std::string *path) {
    int result = 0;
    auto it = pathPlayerMap.find(*path);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            it->second->mGlThread->postMessage(kMsgSurfaceDestroyed);
        } else {
            LOGE("Player is null on destorySurface  path %s", path);
        }
    } else {
        LOGE("Player for path %s not exists on destorySurface", path);
    }
    return result;
}

int MediaController::changeSurfaceSize(std::string *path, int w, int h) {
    int result = 0;
    auto it = pathPlayerMap.find(*path);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            it->second->mGlThread->postMessage(kMsgSurfaceChanged, w, h);
        } else {
            result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
            LOGE("Player is null on changeSurfaceSize  path %s", path);
        }
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s not exists on changeSurfaceSize", path);
    }
    return result;
}

int MediaController::closeStream(std::string *path) {
    int result = 0;
    auto it = pathPlayerMap.find(*path);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            delete (it->second->mFFmpegEncodeStream);
            it->second->mGlThread->quit();
            delete (it->second->mGlThread);
        } else {
            result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
            LOGE("Player is null on changeSurfaceSize  path %s", path);
        }
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s not exists on changeSurfaceSize", path);
    }
    return result;
}

MediaController::~MediaController() {
    for (auto &pair: pathPlayerMap) {
        delete pair.second->mFFmpegEncodeStream;
        delete pair.second->mGlThread;
        delete pair.second;
    }
    pathPlayerMap.clear();
}