//
// Created by hjt on 2025/3/7.
//

#include "../include/GlDrawFbo.h"

Shader
GlDrawFbo::prepareShader(std::string vertexShader, std::string fragmentShader, int w, int h) {
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
        texturePositionLoc = shader->glShader.getAttribLocation("aPosition");
        textureTexCoordLoc = shader->glShader.getAttribLocation("aTexCoord");
        textureSamplerLoc = shader->glShader.getUniformLocation("uTexture");
        return *shader;
    }
}

void GlDrawFbo::release() {
    for (auto [key, val]: shaders) {
        val.glShader.release();
    }
    shaders.clear();
}

void GlDrawFbo::draw(GLint fboTexture, int w, int h,int vw, int vh) {

    // 启用混合模式（叠加智能帧）
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 标准Alpha混合
    Shader mShader = prepareShader(FBO_VERTEX_SHADER_STRING, FBO_FRAGMENT_SHADER_STRING, w, h);
    mShader.glShader.useProgram();
    glViewport(0, 0, vw, vh);
    // 绑定FBO纹理到纹理单元0
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glUniform1i(textureSamplerLoc, 6); // 设置uTexture使用纹理单元0

    glVertexAttribPointer(texturePositionLoc, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(textureTexCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, texCoords);

    glEnableVertexAttribArray(texturePositionLoc);
    glEnableVertexAttribArray(textureTexCoordLoc);

    // 绘制三角形带（4个顶点组成一个四边形）
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

//    // 禁用属性数组
//    glDisableVertexAttribArray(texturePositionLoc);
//    glDisableVertexAttribArray(textureTexCoordLoc);
    // 禁用混合
    glDisable(GL_BLEND);
}