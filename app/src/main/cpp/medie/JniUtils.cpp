//
// Created by hjt on 2025/3/4.
//
#include <jni.h>
#include <stddef.h>
#include <malloc.h>
#include <string>

/*jstring 转char* */
static char *jstringTostr(JNIEnv *env, jstring jstr) {
    char *pStr = NULL;
    jclass jstrObj = env->FindClass("java/lang/String");
    jstring encode = env->NewStringUTF("utf-8");
    jmethodID methodId = env->GetMethodID(jstrObj, "getBytes",
                                          "(Ljava/lang/String;)[B");
    jbyteArray byteArray = (jbyteArray) env->CallObjectMethod(jstr, methodId,
                                                              encode);
    jsize strLen = env->GetArrayLength(byteArray);
    jbyte *jBuf = env->GetByteArrayElements(byteArray, JNI_FALSE);
    if ((char *) jBuf > (char *) 0) {
        pStr = (char *) malloc(strLen + 1);
        if (!pStr) {
            return NULL;
        }
        memcpy(pStr, jBuf, strLen);
        pStr[strLen] = 0;
    }

    env->ReleaseByteArrayElements(byteArray, jBuf, 0);
    env->DeleteLocalRef(byteArray);
    return pStr;
}

static std::string JStringToStdString(JNIEnv *env, jstring jStr) {
    if (!jStr) {
        return "";
    }

    const char *chars = env->GetStringUTFChars(jStr, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(jStr, chars);

    return result;
}
