//
// Created by hjt on 2024/2/18.
//

#include "../include/GlDraw.h"


void GlDraw::perparDrawYuv(int width, int height, YuvData *data, GLuint yuvTextures[3]) {
    int planeWidths[3] = {width, width / 2, width / 2};
    int planeHeights[3] = {height, height / 2, height / 2};

    for (int i = 0; i < 3; i++) {
        yuvTextures[i] = generateTexture(GL_TEXTURE_2D);
    }

    // Upload each plane.
    for (int i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, yuvTextures[i]);
        // GLES only accepts packed data, i.e. stride == planeWidth.
        switch (i) {
            case 0:
                glTexImage2D((GLenum) GL_TEXTURE_2D, (GLint) 0, (GLint) GL_LUMINANCE,
                             (GLsizei) planeWidths[i],
                             (GLsizei) planeHeights[i], (GLint) 0, (GLenum) GL_LUMINANCE,
                             (GLenum) GL_UNSIGNED_BYTE, (GLvoid *) data->y);
                break;
            case 1:
                glTexImage2D((GLenum) GL_TEXTURE_2D, (GLint) 0, (GLint) GL_LUMINANCE,
                             (GLsizei) planeWidths[i],
                             (GLsizei) planeHeights[i], (GLint) 0, (GLenum) GL_LUMINANCE,
                             (GLenum) GL_UNSIGNED_BYTE, (GLvoid *) data->u);
                break;
            case 2:
                glTexImage2D((GLenum) GL_TEXTURE_2D, (GLint) 0, (GLint) GL_LUMINANCE,
                             (GLsizei) planeWidths[i],
                             (GLsizei) planeHeights[i], (GLint) 0, (GLenum) GL_LUMINANCE,
                             (GLenum) GL_UNSIGNED_BYTE, (GLvoid *) data->v);
                break;
        }
    }

}

void GlDraw::drawYuv(GLuint *yuvTextures,
                     int viewportX, int viewportY, int viewportWidth, int viewportHeight,
                     float *scale) {
    drawYuv(yuvTextures, matrix4x4, viewportX, viewportY,
            viewportWidth, viewportHeight, scale);
}

/**
 * Draw a YUV frame with specified texture transformation matrix. Required resources are
 * allocated at the first call to this function.
 */

//
void GlDraw::drawYuv(GLuint *yuvTextures, float *texMatrix,
                     int viewportX, int viewportY, int viewportWidth, int viewportHeight,
                     float *scale) {
    prepareShader(YUV_FRAGMENT_SHADER_STRING, texMatrix, scale);
    //Bind the textures.
    //后面渲染的时候，设置三成纹理
    for (int i = 0; i < 3; ++i) {
        glActiveTexture(
                GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, yuvTextures[i]);
    }
    drawRectangle(viewportX, viewportY, viewportWidth, viewportHeight);
    // Unbind the textures as a precaution..
    glDeleteTextures(4, yuvTextures);
}

void GlDraw::drawRectangle(int x, int y, int width, int height) {
    // Draw quad.
    // 图形绘制
    glViewport(x, y, width, height);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void GlDraw::prepareShader(std::string fragmentShader, float *texMatrix, float *scale) {
    if (shaders.count(fragmentShader) > 0) {
        Shader shader = shaders[fragmentShader];
        shader.glShader.useProgram();
        glUniformMatrix4fv(shader.texMatrixLocation, 1, false, texMatrix);
        glUniform2f(shader.scaleLocation, scale[0], scale[1]);
    } else {
        // Lazy allocation.
        char *s = (char *) VERTEX_SHADER_STRING.data();
        char *s1 = (char *) fragmentShader.c_str();
        GlRendering *mGlRendering = new GlRendering(s, s1);
        int texMatrixLocation = mGlRendering->getUniformLocation("texMatrix");
        int scaleLocation = mGlRendering->getUniformLocation("scale");
        Shader *shader = new Shader();
        shader->glShader = *mGlRendering;
        shader->texMatrixLocation = texMatrixLocation;
        shader->scaleLocation = scaleLocation;
        shaders[fragmentShader] = *shader;
        shader->glShader.useProgram();
        // Initialize fragment shader uniform values.
        if (YUV_FRAGMENT_SHADER_STRING == fragmentShader) {
            //对这几个纹理采样器变量进行设置
            glUniform1i(shader->glShader.getUniformLocation("y_tex"), 0);
            glUniform1i(shader->glShader.getUniformLocation("u_tex"), 1);
            glUniform1i(shader->glShader.getUniformLocation("v_tex"), 2);
        }
        checkNoGLES2Error("Initialize fragment shader uniform values.");
        // Initialize vertex shader attributes.  设置Vertex Shader数据
        // 顶点位置数据传入着色器
        shader->glShader.setVertexAttribArray("in_pos", 2, 0, FULL_RECTANGLE_BUF);
        checkNoGLES2Error("in_pos.");
        shader->glShader.setVertexAttribArray("in_tc", 2, 0, FULL_RECTANGLE_TEX_BUF);
        checkNoGLES2Error("in_tc.");
        shader->glShader.useProgram();
        glUniformMatrix4fv(shader->texMatrixLocation, 1, false, texMatrix);
        glUniform2f(shader->scaleLocation, scale[0], scale[1]);
    }


}

/**
 * Release all GLES resources. This needs to be done manually, otherwise the resources are leaked.
 */


void GlDraw::release() {
    for (auto [key, val]: shaders) {
        val.glShader.release();

    }
    shaders.clear();
}
