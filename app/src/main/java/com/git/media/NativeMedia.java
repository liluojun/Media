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

    public static native int openStream(String path);

    public static native int closeStream(String path);

    public static native int screenshot(String path,String imagePath);

    public static native int creatSurface(String path, Object surface, int w, int h);

    public static native int changeSurfaceSize(String path, int w, int h);

    public static native int destorySurface(String path);

    public static native int init();
    public static native int playbackSpeed(String path, double speed);
    public static native String creatM3u8File(String path, String tsInfoArray);

    public static native int m3u8ToMp4(String path, String outpath);

}
