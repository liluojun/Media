//
// Created by hjt on 2025/3/7.
//

#ifndef MEDIA_GLDRAWFBO_H
#define MEDIA_GLDRAWFBO_H

#include <iostream>
#include <map>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#include "native_log.h"
#include "EglUtils.cpp"
#include "Shader.h"
#include "GlShaderText.h"
#include "AiLineHelper.h"
class GlDrawFbo {
    std::map<std::string, Shader> shaders;

    Shader prepareShader( std::string vertexShader,std::string fragmentShader, int w, int h);
    GLint texturePositionLoc;
    GLint textureTexCoordLoc;
    GLint textureSamplerLoc;

public:
    void draw(GLint fboTexture,int w, int h,int vw, int vh);

    void release();


};



#endif //MEDIA_GLDRAWFBO_H
