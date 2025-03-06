//
// Created by hjt on 2025/3/6.
//

#ifndef MEDIA_AILINEHELPER_H
#define MEDIA_AILINEHELPER_H
typedef enum {
    LINE_SEGMENT, LINE_STRIP, LINE_LOOP, TEXT, RECTANGLE

} DrawType;

struct AiLineData {
    ~AiLineData() {
        if (vertices != nullptr) {
            delete[] vertices;
            vertices = nullptr;
        }
    }

    void coordinateToOpenGL(const float *src, int count) {
        int new_size = (count / 2) * 3; // 计算3D顶点数组大小

        // 内存检查并分配
        if (buffer_size < new_size) {
            delete[] vertices;
            vertices = new float[new_size];
            buffer_size = new_size;
        }

        // 坐标转换并填充 3D 顶点数据
        for (int i = 0, j = 0; i < count; i += 2, j += 3) {
            vertices[j] = (src[i] - 0.5f) * 2.0f;       // X 坐标
            vertices[j + 1] = (src[i + 1] - 0.5f) * 2.0f; // Y 坐标
            vertices[j + 2] = 0.0f;                      // Z 坐标（默认 0）
        }
    }


    DrawType drawType;
    float *vertices;    // 动态数组指针
    int buffer_size;    // 数组总容量
    float color[4];
    int lineWidth;
};

class AiLineHelper {
public:
    void creatAiLineData(AiLineData *mAiLineData);
};

#endif //MEDIA_AILINEHELPER_H
