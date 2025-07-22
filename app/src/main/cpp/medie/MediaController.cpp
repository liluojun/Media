//
// Created by hjt on 2025/2/26.
//
#include "MediaController.h"

int MediaController::openStream(std::string *uuid, std::string *path,int streamType) {
    int result = 0;
    auto it = pathPlayerMap.find(*uuid);
    if (it == pathPlayerMap.end()) {
        Player *player = new Player();
        player->mFFmpegEncodeStream = new EncodeNakedStream();
        player->mGlThread = new GlThread();
        pathPlayerMap[*uuid] = player;
        player->mFFmpegEncodeStream->openStream(stringToChar(*path).data(),streamType);
        player->setupCallback();
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s already exists", uuid);
    }
    return result;
}

int MediaController::pushFrameRaw(std::string *uuid, const uint8_t *buffer, size_t totalSize) {
    int result = 0;
    auto it = pathPlayerMap.find(*uuid);
    if (it == pathPlayerMap.end()) {
        it->second->mFFmpegEncodeStream->pushFrameRaw(buffer, totalSize);
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s already exists", uuid);
    }
    return result;
}

int MediaController::creatSurface(std::string *uuid, ANativeWindow *mWindow, int w, int h) {
    int result = 0;
    auto it = pathPlayerMap.find(*uuid);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            it->second->mGlThread->postMessage(kMsgSurfaceCreated, w, h, mWindow);
        } else {
            result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
            LOGE("Player is null on creatSurface  path %s", uuid);
        }
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s not exists on creatSurface", uuid);
    }
    return result;
}

int MediaController::destorySurface(std::string *uuid) {
    int result = 0;
    auto it = pathPlayerMap.find(*uuid);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            it->second->mGlThread->postMessage(kMsgSurfaceDestroyed);
        } else {
            LOGE("Player is null on destorySurface  path %s", uuid);
        }
    } else {
        LOGE("Player for path %s not exists on destorySurface", uuid);
    }
    return result;
}

int MediaController::changeSurfaceSize(std::string *uuid, int w, int h) {
    int result = 0;
    auto it = pathPlayerMap.find(*uuid);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            it->second->mGlThread->postMessage(kMsgSurfaceChanged, w, h);
        } else {
            result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
            LOGE("Player is null on changeSurfaceSize  path %s", uuid);
        }
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s not exists on changeSurfaceSize", uuid);
    }
    return result;
}

int MediaController::closeStream(std::string *uuid) {
    int result = 0;
    auto it = pathPlayerMap.find(*uuid);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            it->second->mFFmpegEncodeStream->closeStream();
            delete (it->second->mFFmpegEncodeStream);
            it->second->mFFmpegEncodeStream = nullptr;
            it->second->mGlThread->quit();
            delete (it->second->mGlThread);
            it->second->mGlThread = nullptr;
            delete it->second;
            it->second = nullptr;
        } else {
            result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
            LOGE("Player is null on closeStream  path %s", uuid);
        }
        pathPlayerMap.erase(it);
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s not exists on closeStream", uuid);
    }
    return result;
}

int MediaController::playbackSpeed(std::string *uuid, double speed) {
    int result = 0;
    auto it = pathPlayerMap.find(*uuid);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            it->second->mFFmpegEncodeStream->playbackSpeed(speed);
        } else {
            result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
            LOGE("Player is null on playbackSpeed  path %s", uuid);
        }
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s not exists on playbackSpeed", uuid);
    }
    return result;
}

int MediaController::screenshot(std::string *uuid, std::string *imagePath) {
    int result = 0;
    auto it = pathPlayerMap.find(*uuid);
    if (it != pathPlayerMap.end()) {
        if (it->second != nullptr) {
            LOGE("Player is null on screenshot  path %s", imagePath->c_str());
            it->second->mGlThread->postMessage(kMsgScreenShot, new std::string(*imagePath));
        } else {
            result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
            LOGE("Player is null on screenshot  path %s", uuid);
        }
    } else {
        result = ERROR_CODE_TO_INT(ErrorCode::PATH_ALREADY_EXIST);
        LOGE("Player for path %s not exists on screenshot", uuid);
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

const char *MediaController::creatM3u8File(std::string *path, const char *tsList) {
    cJSON *root = cJSON_Parse(tsList);
    if (root == NULL) {
        LOGE("creatM3u8File cJSON_Parse error");
        return "";
    }
    int maxTime = 0;
    int size = cJSON_GetArraySize(root);
    if (size <= 0) {
        LOGE("creatM3u8File ts size<=0");
        return "";
    }
    std::vector<TsInfo> tslist;
    for (int i = 0; i < size; ++i) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        cJSON *time = cJSON_GetObjectItem(item, "time");
        cJSON *url = cJSON_GetObjectItem(item, "url");
        if (maxTime < time->valuedouble) {
            maxTime = time->valuedouble;
        }
        std::string tsPath = *path;
        tsPath.append("/").append(url->valuestring);
        TsInfo tsInfo = {tsPath, time->valuedouble};
        tslist.push_back(tsInfo);

    }
    cJSON_Delete(root);
    std::string m3u8path;
    if (path) {
        m3u8path = *path;
        m3u8path.append("/").append("creat.m3u8");
    }
    FILE *fp = fopen(m3u8path.c_str(), "w");
    if (fp == NULL) {
        LOGE("creatM3u8File openFile error");
        return "";
    }
    // 写入m3u8头部信息
    fprintf(fp, "#EXTM3U\n");
    fprintf(fp, "#EXT-X-VERSION:3\n");
    fprintf(fp, "#EXT-X-TARGETDURATION:%d\n", maxTime);
    fprintf(fp, "#EXT-X-MEDIA-SEQUENCE:1\n");
    for (int i = 0; i < tslist.size(); ++i) {
        fprintf(fp, "#EXTINF:%.1f,\n", tslist[i].timestamp);
        fprintf(fp, "%s\n", tslist[i].url.c_str());
    }

    // 写入结束标志
    fprintf(fp, "#EXT-X-ENDLIST\n");

    // 关闭文件
    fclose(fp);

    return m3u8path.c_str();
}

int MediaController::m3u8ToMp4(const char *input_path, const char *output_path) {
    FFmpegStreamToMP4 *ffmpegStreamToMP4 = new FFmpegStreamToMP4();
    ffmpegStreamToMP4->streamToMP4(input_path, output_path, NULL);
    delete (ffmpegStreamToMP4);
    return 0;
}
