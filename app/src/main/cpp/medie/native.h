//
// Created by hjt on 2025/3/4.
//

#ifndef MEDIA_NATIVE_H
#define MEDIA_NATIVE_H
#endif
#ifdef __cplusplus
extern "C"
{
#endif

JNIEXPORT jint JNICALL openStream(JNIEnv *env, jobject thiz, jstring uuid, jstring path);
JNIEXPORT jint JNICALL
creatSurface(JNIEnv *env, jobject thiz, jstring uuid, jobject mWindow, jint w, jint h);

JNIEXPORT jint JNICALL destorySurface(JNIEnv *env, jobject thiz, jstring uuid);

JNIEXPORT jint JNICALL changeSurfaceSize(JNIEnv *env, jobject thiz, jstring uuid, jint w, jint h);

JNIEXPORT jint JNICALL closeStream(JNIEnv *env, jobject thiz, jstring uuid);

JNIEXPORT jint JNICALL playbackSpeed(JNIEnv *env, jobject thiz, jstring uuid, jdouble speed);

JNIEXPORT jint JNICALL screenshot(JNIEnv *env, jobject thiz, jstring uuid, jstring imagePath);

JNIEXPORT jint JNICALL init(JNIEnv *env, jobject thiz);
JNIEXPORT jstring JNICALL creatM3u8File(JNIEnv *env, jobject thiz, jstring path, jstring tsInfo);
JNIEXPORT jint JNICALL m3u8ToMp4(JNIEnv *env, jobject thiz, jstring path, jstring outPath);
#ifdef __cplusplus
}
#endif