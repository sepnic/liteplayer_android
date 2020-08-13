/*
 * Copyright (C) 2018-2020 luoyun <sysu.zqlong@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This is a JNI example where we use native methods to play music
 * using the native liteplayer* APIs.
 * See the corresponding Java source file located at:
 *
 *   src/com/example/liteplayerdemo/Liteplayer.java
 *
 * In this example we use assert() for "impossible" error conditions,
 * and explicit handling and recovery for more likely error conditions.
 */

#include <jni.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "msgutils/os_logger.h"
#include "msgutils/Namespace.hpp"
#include "msgutils/Mutex.hpp"
#include "liteplayer/liteplayer_main.h"
#include "liteplayer/liteplayer_adapter.h"
#include "liteplayer/adapter/fatfs_wrapper.h"
#include "liteplayer/adapter/httpclient_wrapper.h"
#include "liteplayer/adapter/opensles_wrapper.h"

#define TAG "NativeLiteplayer"
#define JAVA_CLASS_NAME "com/sepnic/liteplayer/Liteplayer"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

struct fields_t {
    jfieldID    mContext;
    jmethodID   mPostEvent;
    jmethodID   mOpenTrack;
    jmethodID   mWriteTrack;
    jmethodID   mCloseTrack;
    JavaVM*     mJavaVM;
    jclass      mClass;
    jobject     mObject;
};
static fields_t sFields;
static msgutils::Mutex sLock;

static void jniThrowException(JNIEnv* env, const char* className, const char* msg) {
    jclass clazz = env->FindClass(className);
    if (!clazz) {
        OS_LOGE(TAG, "Unable to find exception class %s", className);
        /* ClassNotFoundException now pending */
        return;
    }

    if (env->ThrowNew(clazz, msg) != JNI_OK) {
        OS_LOGE(TAG, "Failed throwing '%s' '%s'", className, msg);
        /* an exception, most likely OOM, will now be pending */
    }
    env->DeleteLocalRef(clazz);
}

static liteplayer_handle_t getLiteplayer(JNIEnv* env, jobject thiz)
{
    msgutils::Mutex::Autolock l(sLock);
    liteplayer_handle_t p = (liteplayer_handle_t)env->GetLongField(thiz, sFields.mContext);
    return p;
}

static void setLiteplayer(JNIEnv* env, jobject thiz, liteplayer_handle_t player)
{
    msgutils::Mutex::Autolock l(sLock);
    env->SetLongField(thiz, sFields.mContext, (jlong)player);
}

sink_handle_t audiotrack_wrapper_open(int samplerate, int channels, void *sink_priv)
{
    OS_LOGD(TAG, "@@@ Opening AudioTrack: samplerate=%d, channels=%d", samplerate, channels);
    JNIEnv *env;
    JavaVMAttachArgs args = { JNI_VERSION_1_6, "LiteplayerAudioTrack", NULL };
    jint res = sFields.mJavaVM->AttachCurrentThread(&env, &args);
    if (res != JNI_OK) {
        OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
        return NULL;
    }
    res = env->CallStaticIntMethod(sFields.mClass, sFields.mOpenTrack, sFields.mObject, samplerate, channels);
    sFields.mJavaVM->DetachCurrentThread();
    return res == 0 ? &sFields : NULL;
}

int audiotrack_wrapper_write(sink_handle_t handle, char *buffer, int size)
{
    OS_LOGV(TAG, "@@@ Writing AudioTrack: buffer=%p, size=%d", buffer, size);
    JNIEnv *env;
    JavaVMAttachArgs args = { JNI_VERSION_1_6, "LiteplayerAudioTrack", NULL };
    jint res = sFields.mJavaVM->AttachCurrentThread(&env, &args);
    if (res != JNI_OK) {
        OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
        return -1;
    }
    jbyteArray sampleArray = env->NewByteArray(size);
    if (sampleArray == NULL) {
        return -1;
    }
    jbyte *sampleByte = env->GetByteArrayElements(sampleArray, NULL);
    memcpy(sampleByte, buffer, size);
    env->ReleaseByteArrayElements(sampleArray, sampleByte, 0);
    res = env->CallStaticIntMethod(sFields.mClass, sFields.mWriteTrack, sFields.mObject, sampleArray, size);
    env->DeleteLocalRef(sampleArray);
    sFields.mJavaVM->DetachCurrentThread();
    return size;
}

void audiotrack_wrapper_close(sink_handle_t handle)
{
    OS_LOGD(TAG, "@@@ closing AudioTrack");
    JNIEnv *env;
    JavaVMAttachArgs args = { JNI_VERSION_1_6, "LiteplayerAudioTrack", NULL };
    jint res = sFields.mJavaVM->AttachCurrentThread(&env, &args);
    if (res != JNI_OK) {
        OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
        return;
    }
    env->CallStaticVoidMethod(sFields.mClass, sFields.mCloseTrack, sFields.mObject);
    sFields.mJavaVM->DetachCurrentThread();
}

static int Liteplayer_native_stateCallback(enum liteplayer_state state, int errcode, void *priv)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_stateCallback: state=%d, errcode=%d", state, errcode);
    JNIEnv *env;
    jint res = sFields.mJavaVM->GetEnv((void**) &env, JNI_VERSION_1_6);
    jboolean attached = JNI_FALSE;
    if (res != JNI_OK) {
        JavaVMAttachArgs args = { JNI_VERSION_1_6, "LiteplayerStateCallback", NULL };
        res = sFields.mJavaVM->AttachCurrentThread(&env, &args);
        if (res != JNI_OK) {
            OS_LOGE(TAG, "Failed to AttachCurrentThread, errcode=%d", res);
            return -1;
        }
        attached = true;
    }

    env->CallStaticVoidMethod(sFields.mClass, sFields.mPostEvent, sFields.mObject, state, errcode);

    if (attached)
        sFields.mJavaVM->DetachCurrentThread();
    return 0;
}

static jint Liteplayer_native_create(JNIEnv* env, jobject thiz, jobject weak_this)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_create");

    jclass clazz;
    clazz = env->FindClass(JAVA_CLASS_NAME);
    if (clazz == NULL) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
        return -1;
    }
    sFields.mContext = env->GetFieldID(clazz, "mNativeContext", "J");
    if (sFields.mContext == NULL) {
        OS_LOGE(TAG, "Failed to get native context");
        return -1;
    }
    sFields.mPostEvent = env->GetStaticMethodID(clazz, "postEventFromNative", "(Ljava/lang/Object;II)V");
    if (sFields.mPostEvent == NULL) {
        OS_LOGE(TAG, "Failed to get postEventFromNative mothod");
        return -1;
    }
    sFields.mOpenTrack = env->GetStaticMethodID(clazz, "openAudioTrackFromNative", "(Ljava/lang/Object;II)I");
    if (sFields.mOpenTrack == NULL) {
        OS_LOGE(TAG, "Failed to get openAudioTrackFromNative mothod");
        return -1;
    }
    sFields.mWriteTrack = env->GetStaticMethodID(clazz, "writeAudioTrackFromNative", "(Ljava/lang/Object;[BI)I");
    if (sFields.mWriteTrack == NULL) {
        OS_LOGE(TAG, "Failed to get writeAudioTrackFromNative mothod");
        return -1;
    }
    sFields.mCloseTrack = env->GetStaticMethodID(clazz, "closeAudioTrackFromNative", "(Ljava/lang/Object;)V");
    if (sFields.mCloseTrack == NULL) {
        OS_LOGE(TAG, "Failed to get closeAudioTrackFromNative mothod");
        return -1;
    }
    env->DeleteLocalRef(clazz);

    // Hold onto Liteplayer class for use in calling the static method
    // that posts events to the application thread.
    clazz = env->GetObjectClass(thiz);
    if (clazz == NULL) {
        OS_LOGE(TAG, "Failed to find class: %s", JAVA_CLASS_NAME);
        jniThrowException(env, "java/lang/Exception", NULL);
        return -1;
    }
    sFields.mClass = (jclass)env->NewGlobalRef(clazz);
    // We use a weak reference so the Liteplayer object can be garbage collected.
    // The reference is only used as a proxy for callbacks.
    sFields.mObject  = env->NewGlobalRef(weak_this);

    liteplayer_handle_t p = liteplayer_create();
    if (p == NULL) {
        return -1;
    }
    // Register state listener, todo: notify java objest
    liteplayer_register_state_listener(p, Liteplayer_native_stateCallback, NULL);
    // Register sink adapter
    struct sink_wrapper sink_ops = {
            .sink_priv = NULL,
            .open = audiotrack_wrapper_open,
            .write = audiotrack_wrapper_write,
            .close = audiotrack_wrapper_close,
    };
    liteplayer_register_sink_wrapper(p, &sink_ops);
    // Register http adapter
    struct file_wrapper file_ops = {
            .file_priv = NULL,
            .open = fatfs_wrapper_open,
            .read = fatfs_wrapper_read,
            .filesize = fatfs_wrapper_filesize,
            .seek = fatfs_wrapper_seek,
            .close = fatfs_wrapper_close,
    };
    liteplayer_register_file_wrapper(p, &file_ops);
    // Register file adapter
    struct http_wrapper http_ops = {
            .http_priv = NULL,
            .open = httpclient_wrapper_open,
            .read = httpclient_wrapper_read,
            .filesize = httpclient_wrapper_filesize,
            .seek = httpclient_wrapper_seek,
            .close = httpclient_wrapper_close,
    };
    liteplayer_register_http_wrapper(p, &http_ops);

    // Stow our new C++ MediaPlayer in an opaque field in the Java object.
    setLiteplayer(env, thiz, p);
    return 0;
}

static jint Liteplayer_native_setDataSource(JNIEnv *env, jobject thiz, jstring path)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_setDataSource");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return -1;
    }
    if (path == NULL) {
        jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
        return -1;
    }
    const char *tmp = env->GetStringUTFChars(path, NULL);
    if (tmp == NULL) {
        jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return -1;
    }
    std::string url = tmp;
    env->ReleaseStringUTFChars(path, tmp);
    return (jint) liteplayer_set_data_source(p, url.c_str(), 0);
}

static jint Liteplayer_native_prepareAsync(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_prepareAsync");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return -1;
    }
    return (jint) liteplayer_prepare_async(p);
}

static jint Liteplayer_native_start(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_start");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return -1;
    }
    return (jint) liteplayer_start(p);
}

static jint Liteplayer_native_pause(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_pause");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return -1;
    }
    return (jint) liteplayer_pause(p);
}

static jint Liteplayer_native_resume(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_resume");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return -1;
    }
    return (jint) liteplayer_resume(p);
}

static jint Liteplayer_native_seekTo(JNIEnv *env, jobject thiz, jint msec)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_seekTo");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return -1;
    }
    return (jint) liteplayer_seek(p, msec);
}

static jint Liteplayer_native_stop(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_stop");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return -1;
    }
    return (jint) liteplayer_stop(p);
}

static jint Liteplayer_native_reset(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_reset");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return -1;
    }
    return (jint) liteplayer_reset(p);
}

static jint Liteplayer_native_getCurrentPosition(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_getCurrentPosition");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    int msec = 0;
    liteplayer_get_position(p, &msec);
    return (jint)msec;
}

static jint Liteplayer_native_getDuration(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_getCurrentPosition");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    int msec = 0;
    liteplayer_get_duration(p, &msec);
    return (jint)msec;
}

static void Liteplayer_native_destroy(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_destroy");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    setLiteplayer(env, thiz, 0);
    liteplayer_destroy(p);

    // remove global references
    env->DeleteGlobalRef(sFields.mObject);
    env->DeleteGlobalRef(sFields.mClass);
    sFields.mObject = NULL;
    sFields.mClass = NULL;
}

static JNINativeMethod gMethods[] = {
        {"native_create", "(Ljava/lang/Object;)I", (void *)Liteplayer_native_create},
        {"native_destroy", "()V", (void *)Liteplayer_native_destroy},
        {"native_setDataSource", "(Ljava/lang/String;)I", (void *)Liteplayer_native_setDataSource},
        {"native_prepareAsync", "()I", (void *)Liteplayer_native_prepareAsync},
        {"native_start", "()I", (void *)Liteplayer_native_start},
        {"native_pause", "()I", (void *)Liteplayer_native_pause},
        {"native_resume", "()I", (void *)Liteplayer_native_resume},
        {"native_seekTo", "(I)I", (void *)Liteplayer_native_seekTo},
        {"native_stop", "()I", (void *)Liteplayer_native_stop},
        {"native_reset", "()I", (void *)Liteplayer_native_reset},
        {"native_getCurrentPosition", "()I", (void *)Liteplayer_native_getCurrentPosition},
        {"native_getDuration", "()I", (void *)Liteplayer_native_getDuration},
};

static int registerNativeMethods(JNIEnv* env, const char* className,JNINativeMethod* getMethods,int methodsNum){
    jclass clazz;
    clazz = env->FindClass(className);
    if(clazz == NULL){
        return JNI_FALSE;
    }
    if(env->RegisterNatives(clazz,getMethods,methodsNum) < 0){
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        OS_LOGE(TAG, "Failed to get env");
        goto bail;
    }
    assert(env != NULL);

    if (registerNativeMethods(env, JAVA_CLASS_NAME, gMethods, NELEM(gMethods)) != JNI_TRUE) {
        OS_LOGE(TAG, "Failed to register native methods");
        goto bail;
    }

    sFields.mJavaVM = vm;
    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

bail:
    return result;
}
