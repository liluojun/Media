//
// Created by hjt on 2025/2/26.
//
#include <string>
#include <vector>
#include <cstring>
#include <chrono>
static std::vector<char> stringToChar(const std::string& strPath) {
    std::vector<char> cpath(strPath.size() + 1);
    strcpy(cpath.data(), strPath.c_str());
    return cpath;
}


// 获取毫秒时间戳 (UTC时区)
static int64_t getTimestampMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    ).count();
}