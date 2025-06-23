//
// Created by hjt on 2024/2/4.
//

#include "../include/GlRendering.h"

GlRendering::GlRendering() {

}

GLint GlRendering::getFBO() {
    return fbo;
}
GLint GlRendering::getFboTexture() {
    return fboTexture;
}
GlRendering::GlRendering(char *vertexSource, char *fragmentSource) {
    const char *version = (const char *) glGetString(GL_VERSION);
    LOGE("GL_VERSION: %s", version);
    GlShader(vertexSource, fragmentSource);
}

int compileShader(int shaderType, char *source) {
    GLuint shader = glCreateShader(shaderType);
    GLint compileStatus;
    if (shader == 0) {
        LOGE("glCreateShader() failed. GLES20 error:%d ", glGetError());
        return -1;
    }
    //将源码添加到shader并编译之

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus != GL_TRUE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        char *log = (char *) malloc(logLength);
        glGetShaderInfoLog(shader, logLength, nullptr, log);
        LOGE("compileShader() failed. GLES20 error:%s ", log);
        free(log);
        return -2;
    }
    checkNoGLES2Error("compileShader");
    return shader;
}


int GlRendering::GlShader(char *vertexSource, char *fragmentSource) {
    int vertexShader = compileShader(
            GL_VERTEX_SHADER, vertexSource);
    int fragmentShader = compileShader(
            GL_FRAGMENT_SHADER, fragmentSource);
    program = glCreateProgram();
    if (program == 0) {
        LOGE("glCreateShader() failed. GLES20 error:%d ", glGetError());
        return -1;
    }
// 将vertexShader shader添加到program
    glAttachShader(program, vertexShader);
// 将fragmentShader shader添加到program
    glAttachShader(program, fragmentShader);
// 创建可执行的 OpenGL ES program（耗CPU）
    glLinkProgram(program);
    GLint compileStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &compileStatus);
    if (compileStatus != GL_TRUE) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        char *log = (char *) malloc(logLength);
        glGetProgramInfoLog(program, logLength, nullptr, log);
        LOGE("glCreateShader() failed. GLES20 error:%s ", log);
        free(log);
        return -2;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    checkNoGLES2Error("Creating GlShader");
    return 0;
}

GLint GlRendering::getAttribLocation(char *label) {
    if (program == -1) {
        LOGE("The program has been released");
        return -1;
    }
    GLint location = glGetAttribLocation(program, label);
    if (location < 0) {
        //LOGE("Could not locate %d in program", label);
        return -2;
    }
    return location;
}


/**
 * Enable and upload a vertex array for attribute |label|. The vertex data is specified in
 * |buffer| with |dimension| number of components per vertex and specified |stride|.
 */
GLint GlRendering::setVertexAttribArray(GLchar *label,
                                        int dimension,
                                        int stride, const void *pointer) {
    if (program == -1) {
        LOGE("The program has been released");
        return -1;
    }
    GLint location = getAttribLocation(label);
// 允许使用顶点坐标数组
    glEnableVertexAttribArray(location);
// 顶点坐标传递到顶点着色器
    glVertexAttribPointer(location, dimension, GL_FLOAT, false, stride, pointer);
    return location;
}

/**
/**
 //获取shader里面uniform变量的地址
 * @param label
 * @return
 */
GLint GlRendering::getUniformLocation(GLchar *label) {
    if (program == -1) {
        LOGE("The program has been released");
        return -1;
    }
// 获取指向着色器中label的index
    GLint location = glGetUniformLocation(program, label);
    if (location < 0) {
        //LOGE("Could not locate uniform %d in program", location);
        return -1;
    }
    return location;
}

void GlRendering::useProgram() {
    if (program == -1) {
        LOGE("The program has been released");
        return;
    }
    // 使用shader程序
    glUseProgram(program);
    checkNoGLES2Error("glUseProgram");
}

void GlRendering::release() {
    // Delete program, automatically detaching any shaders from it.
    if (program != -1) {
        glDeleteProgram(program);
        program = -1;
    }
}

void GlRendering::initFBO(int width, int height) {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &fboTexture);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // 必须设置纹理参数！
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // 打印错误信息
        LOGE("glCheckFramebufferStatus() failed");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
}


