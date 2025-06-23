//
// Created by hjt on 2025/4/12.
//

#ifndef MEDIA_FRAMETYPE_H
#define MEDIA_FRAMETYPE_H
typedef enum
{
    //! 主码流帧类型
    PktError = 0x00,
    PktIFrames = 0x01,
    PktAudioFrames = 0x08,
    PktPFrames = 0x09,
    PktBBPFrames = 0x0a,
    PktMotionDetection = 0x0b,
    PktDspStatus = 0x0c,
    PktOrigImage = 0x0d,
    PktSysHeader = 0x0e,
    PktBPFrames = 0x0f,
    PktSFrames = 0x10,
    //! 子码流帧类型
    PktSubSysHeader = 0x11,
    PktSubIFrames = 0x12,
    PktSubPFrames = 0x13,
    PktSubBBPFrames = 0x14,
    //! 智能分析信息帧类型
    PktVacEventZones = 0x15,
    PktVacObjects = 0x16,
    // zhangw:这是在接入智能帧数据时加入的
    //! 第三码流帧类型
    PktThirdSysHeader = 0x17,
    PktThirdIFrames = 0x18,
    PktThirdPFrames = 0x19,
    PktThirdBBPFrames = 0x1a,

    //! 智能检测帧类型
    PktSmartIFrames = 0x1b,
    PktSmartPFrames = 0x1c,
    //! APP metedata帧.
    PktAppFrames = 0x20,
    //! Comb帧
    PktCombFrames = 0x21,

    PktVirtualIFrames = 0xa1,
    PktVirtualSubIFrames = 0xa2,
} aliIoT_frame_type_t;
#endif //MEDIA_FRAMETYPE_H
