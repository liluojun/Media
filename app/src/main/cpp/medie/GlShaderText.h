//
// Created by hjt on 2025/3/6.
//

#ifndef MEDIA_GLSHADERTEXT_H
#define MEDIA_GLSHADERTEXT_H

#include <iostream>

/*******************智能帧顶点片源着色器*****************************/
static const std::string AI_VERTEX_SHADER_STRING =
        "#version 100\n"
        "attribute vec4 vPosition;\n"
        "uniform vec2 scale;\n"  // 新增缩放参数，x,y方向缩放
        "void main() {\n"
        " vec4 scaled_pos = vPosition;\n"
        "    scaled_pos.x *= scale.x;\n"
        "    scaled_pos.y *= scale.y;"
        "    gl_Position = scaled_pos;\n"
        "}";
static const std::string AI_FRAGMENT_SHADER_STRING =
        "precision mediump float;\n"
        "\n"
        "uniform vec4 vColor;\n"
        "\n"
        "void main() {\n"
        "    gl_FragColor = vColor;\n"
        "}\n";
/*******************智能帧顶点片源着色器 end*****************************/
/*******************YUV顶点片源着色器以及相关顶点坐标*****************************/
static const std::string VERTEX_SHADER_STRING =
        "varying vec2 interp_tc;\n"
        "uniform vec2 scale;\n"// 用于缩放顶点坐标;
        "attribute vec4 in_pos;\n"
        "attribute vec4 in_tc;\n"
        "\n"
        "uniform mat4 texMatrix;\n"
        "\n"
        "void main() {\n"
        " vec4 scaled_pos = in_pos;\n"
        "    scaled_pos.x *= scale.x;\n"
        "    scaled_pos.y *= scale.y;"
        "    gl_Position = scaled_pos;\n"
        "    interp_tc = (texMatrix * in_tc).xy;\n"
        "}\n";
static const std::string YUV_FRAGMENT_SHADER_STRING =
        "precision mediump float;\n"
        "varying vec2 interp_tc;\n"
        "\n"
        "uniform sampler2D y_tex;\n"
        "uniform sampler2D u_tex;\n"
        "uniform sampler2D v_tex;\n"
        "\n"
        "void main() {\n"
        "  float y = texture2D(y_tex, interp_tc).r;\n"
        "  float u = texture2D(u_tex, interp_tc).r - 0.5;\n"
        "  float v = texture2D(v_tex, interp_tc).r - 0.5;\n"
        "  gl_FragColor = vec4(y + 1.403 * v, "
        "                      y - 0.344 * u - 0.714 * v, "
        "                      y + 1.77 * u, 1);\n"
        "}\n";

static const float FULL_RECTANGLE_BUF[8] = {-1.0f, -1.0f, // Bottom left.
                                            1.0f, -1.0f, // Bottom right.
                                            -1.0f, 1.0f, // Top left.
                                            1.0f, 1.0f, // Top right.
};

// Texture coordinates - (0, 0) is bottom-left and (1, 1) is top-right.
static const float FULL_RECTANGLE_TEX_BUF[8] = {
        0.0f, 0.0f, // Bottom left.
        1.0f, 0.0f, // Bottom right.
        0.0f, 1.0f, // Top left.
        1.0f, 1.0f // Top right.
};

static float matrix4x4[16] = {1.0, 0.0, 0.0, 0.0,
                              -0.0, -1.0, 0.0, -0.0,
                              0.0, 0.0, 1.0, 0.0,
                              0.0, 1.0, 0.0, 1.0};
/*******************YUV顶点片源着色器 END*****************************/
/********************FBO混合着色器******************************/
// 顶点着色器
static const char *FBO_VERTEX_SHADER_STRING =
        "attribute vec4 aPosition;\n"
        "attribute vec2 aTexCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_Position = aPosition;\n"
        "    vTexCoord = aTexCoord;\n"
        "}";

// 片段着色器
static const char *FBO_FRAGMENT_SHADER_STRING =
        "precision mediump float;\n"
        "varying vec2 vTexCoord;\n"
        "uniform sampler2D uTexture;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(uTexture, vTexCoord);\n"
        "}";
// 顶点坐标（NDC坐标系，覆盖整个屏幕）
static const GLfloat vertices[] = {
        -1.0f, -1.0f,  // 左下
        1.0f, -1.0f,  // 右下
        -1.0f, 1.0f,  // 左上
        1.0f, 1.0f   // 右上
};

// 纹理坐标（与顶点对应）
static const GLfloat texCoords[] = {
        0.0f, 0.0f,    // 左下
        1.0f, 0.0f,    // 右下
        0.0f, 1.0f,    // 左上
        1.0f, 1.0f     // 右上
};
/********************FBO混合着色器******************************/
#endif //MEDIA_GLSHADERTEXT_H
