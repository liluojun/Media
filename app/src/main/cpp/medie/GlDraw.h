//
// Created by hjt on 2024/2/18.
//

#ifndef PLAYVIDEO_GLDRAW_H
#define PLAYVIDEO_GLDRAW_H

#include "YuvData.h"
#include "Shader.h"
#include "EglUtils.cpp"
#include <iostream>
#include <map>
#include "GlShaderText.h"


class GlDraw {
public:

    std::map<std::string, Shader> shaders;

    void prepareShader(std::string fragmentShader, float *texMatrix);

    void drawYuv(GLuint yuvTextures[3], float *texMatrix,
                 int viewportX, int viewportY, int viewportWidth, int viewportHeight);

    void drawRectangle(int x, int y, int width, int height);

    void release();

    void drawYuv(GLuint yuvTextures[3],
                 int viewportX, int viewportY, int viewportWidth, int viewportHeight);

    void perparDrawYuv(int width, int height, YuvData *data, GLuint yuvTextures[3]);
};


#endif //PLAYVIDEO_GLDRAW_H
