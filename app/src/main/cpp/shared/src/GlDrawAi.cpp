//
// Created by hjt on 2025/3/5.
//

#include "../include/GlDrawAi.h"
#include "../include/AiLineHelper.h"
static_assert(sizeof(AiLineData) > 0, "AiLineData must be a complete type");
Shader
GlDrawAi::prepareShader(std::string vertexShader, std::string fragmentShader, int w, int h) {
    if (shaders.count(fragmentShader) > 0) {
        Shader shader = shaders[fragmentShader];
        return shader;
    } else {
        char *s = (char *) vertexShader.c_str();
        char *s1 = (char *) fragmentShader.c_str();
        GlRendering *mGlRendering = new GlRendering(s, s1);
        mGlRendering->initFBO(w, h);
        fboTextureId = mGlRendering->getFboTexture();
        Shader *shader = new Shader();
        shader->glShader = *mGlRendering;
        shaders[fragmentShader] = *shader;
        return *shader;
    }
}

GLint GlDrawAi::getFboTexture() {
    return fboTextureId;
}

void GlDrawAi::release() {
    for (auto [key, val]: shaders) {
        val.glShader.release();
    }
    shaders.clear();
}

void GlDrawAi::drawAi(std::vector<AiLineData> &pData, int w, int h, int vw, int vh, float *scale) {
    Shader mShader = prepareShader(AI_VERTEX_SHADER_STRING, AI_FRAGMENT_SHADER_STRING, vw, vh);
    glBindFramebuffer(GL_FRAMEBUFFER, mShader.glShader.getFBO());
    glViewport(0, 0, vw, vh);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    mShader.glShader.useProgram();
    for (const AiLineData& data : pData) {
        GLint result = mShader.glShader.setVertexAttribArray("vPosition", 3, 0, (data.vertices));
        if (result == -1) {
            return;
        }
        GLint mColorHandle = mShader.glShader.getUniformLocation("vColor");
        if (mColorHandle == -1) {
            return;
        }
        int scaleLoc = mShader.glShader.getUniformLocation("scale");
        if (scaleLoc != -1) {
            glUniform2fv(scaleLoc, 1, scale);  // 传入float[2]的缩放值
        }

        glUniform4fv(mColorHandle, 1, data.color);
        glLineWidth(data.lineWidth);
        glDrawArrays(GL_LINES, 0, data.buffer_size / 3);
        glDisableVertexAttribArray(result);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

