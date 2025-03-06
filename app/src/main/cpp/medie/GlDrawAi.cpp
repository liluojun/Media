//
// Created by hjt on 2025/3/5.
//

#include "GlDrawAi.h"


Shader
GlDrawAi::prepareShader(std::string vertexShader,std::string fragmentShader) {
    if (shaders.count(fragmentShader) > 0) {
        Shader shader = shaders[fragmentShader];
        return shader;
    } else {
        char *s = (char *) vertexShader.c_str();
        char *s1 = (char *) fragmentShader.c_str();
        GlRendering *mGlRendering = new GlRendering(s, s1);
        Shader *shader = new Shader();
        shader->glShader = *mGlRendering;
        shaders[fragmentShader] = *shader;
        return *shader;
    }
}

void GlDrawAi::release() {
    for (auto [key, val]: shaders) {
        val.glShader.release();
    }
    shaders.clear();
}

void GlDrawAi::drawAi(AiLineData *pData) {
    switch (pData->drawType) {
        case LINE_SEGMENT: {
            drawSegment(pData);
            break;
        }
      /*  case LINE_STRIP: {
            drawStrip(pData);
            break;
        }
        case LINE_LOOP: {
            drawLoop(pData);
            break;
        }
        case TEXT: {
            drawText(pData);
            break;
        }
        case RECTANGLE: {
            drawRectangle(pData);
            break;
        }*/

    }

}
void GlDrawAi::drawSegment(AiLineData *pData) {
    Shader mShader = prepareShader(AI_VERTEX_SHADER_STRING, AI_FRAGMENT_SHADER_STRING);
    mShader.glShader.useProgram();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLint result = mShader.glShader.setVertexAttribArray("vPosition", 3, 0, (pData->vertices));
    if (result == -1) {
        return;
    }
    GLint mColorHandle = mShader.glShader.getUniformLocation("vColor");
    if (mColorHandle == -1) {
        return;
    }
    glUniform4fv(mColorHandle, 1, pData->color);
    glLineWidth(pData->lineWidth);
    glDrawArrays(GL_LINES, 0, pData->buffer_size / 3);
    glDisableVertexAttribArray(result);
}