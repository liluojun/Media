package com.git.media;

import android.view.Surface;

public class NativeMedia {

    static {
        System.loadLibrary("medie");
        System.loadLibrary("avcodec");
        System.loadLibrary("avfilter");
        System.loadLibrary("avformat");
        System.loadLibrary("avutil");
        System.loadLibrary("swresample");
        System.loadLibrary("swscale");
    }

    public static native int openStream(String uuid, String path,int streamType);

    public static native int closeStream(String uuid);

    public static native int screenshot(String uuid, String imagePath);

    public static native int creatSurface(String uuid, Object surface, int w, int h);

    public static native int changeSurfaceSize(String uuid, int w, int h);

    public static native int destorySurface(String uuid);

    public static native int init();

    public static native int playbackSpeed(String path, double speed);

    public static native int pushFrameRaw(String uuid, byte[] byteArray);

    public static native String creatM3u8File(String path, String tsInfoArray);

    public static native int m3u8ToMp4(String path, String outpath);

}
