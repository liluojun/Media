//
// Created by hjt on 2025/3/5.
//

#ifndef MEDIA_GLDRAWAI_H
#define MEDIA_GLDRAWAI_H

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

class GlDrawAi {
    std::map<std::string, Shader> shaders;

    Shader prepareShader( std::string vertexShader,std::string fragmentShader);


public:
    void drawAi(AiLineData *pData);

    void release();

    void drawSegment(AiLineData *pData);
};


#endif //MEDIA_GLDRAWAI_H
