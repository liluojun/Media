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

JNIEXPORT jint JNICALL openStream(JNIEnv *env, jobject thiz, jstring path);
JNIEXPORT jint JNICALL
creatSurface(JNIEnv *env, jobject thiz, jstring path, jobject mWindow, jint w, jint h);

JNIEXPORT jint JNICALL destorySurface(JNIEnv *env, jobject thiz, jstring path);

JNIEXPORT jint JNICALL changeSurfaceSize(JNIEnv *env, jobject thiz, jstring path, jint w, jint h);

JNIEXPORT jint JNICALL closeStream(JNIEnv *env, jobject thiz, jstring path);
JNIEXPORT jint JNICALL init(JNIEnv *env, jobject thiz);
#ifdef __cplusplus
}
#endif