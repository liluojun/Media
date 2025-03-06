//
// Created by hjt on 2025/3/6.
//

#ifndef MEDIA_SHADER_H
#define MEDIA_SHADER_H
#include "GlRendering.h"
typedef struct Shader {
    GlRendering glShader;
    int texMatrixLocation;
} Shader;

#endif //MEDIA_SHADER_H
