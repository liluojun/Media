//
// Created by hjt on 2025/2/26.
//
#include <string>
#include <vector>
#include <cstring>

static std::vector<char> stringToChar(const std::string& strPath) {
    std::vector<char> cpath(strPath.size() + 1);
    strcpy(cpath.data(), strPath.c_str());
    return cpath;
}
