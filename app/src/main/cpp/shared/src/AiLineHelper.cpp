//
// Created by hjt on 2025/3/6.
//
#include "../include/AiLineHelper.h"

void AiLineHelper::creatAiLineData(AiLineData *mAiLineData) {

    mAiLineData->drawType = LINE_SEGMENT;
    float raw_coords[] ={0.0f, 0.5f, 1.0f, 0.5f};
    mAiLineData->coordinateToOpenGL(raw_coords, sizeof(raw_coords) / sizeof(raw_coords[0]));
    mAiLineData->lineWidth = 5;
    mAiLineData->color[0] = 0.94f;
    mAiLineData->color[1] = 0.0f;
    mAiLineData->color[2] = 0.078f;
    mAiLineData->color[3] = 0.5f;

}
