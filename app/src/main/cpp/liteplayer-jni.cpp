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
#include "liteplayer/adapter/fatfs_wrapper.h"
#include "liteplayer/adapter/httpclient_wrapper.h"
#include "liteplayer/adapter/opensles_wrapper.h"

#define TAG "NativeLiteplayer"
#define JAVA_CLASS_NAME "com/example/liteplayerdemo/Liteplayer"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

struct fields_t {
    jfieldID    mContext;
    jmethodID   mPostEvent;
    JavaVM*     mJavaVM;
    jclass      mClass;
    jobject     mObject;
};
static fields_t fields;
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
    liteplayer_handle_t p = (liteplayer_handle_t)env->GetLongField(thiz, fields.mContext);
    return p;
}

static void setLiteplayer(JNIEnv* env, jobject thiz, liteplayer_handle_t player)
{
    msgutils::Mutex::Autolock l(sLock);
    env->SetLongField(thiz, fields.mContext, (jlong)player);
}

static int Liteplayer_native_stateCallback(enum liteplayer_state state, int errcode, void *priv)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_stateCallback: state=%d, errcode=%d", state, errcode);
    JavaVM *javaVM = fields.mJavaVM;
    JNIEnv *env;
    jint res = javaVM->GetEnv((void**) &env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        OS_LOGE(TAG, "ERROR: GetEnv failed");
        return -1;
    }
    env->CallStaticVoidMethod(fields.mClass, fields.mPostEvent, fields.mObject, state, errcode);
    return 0;
}

static void Liteplayer_native_create(JNIEnv* env, jobject thiz, jobject weak_this)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_create");

    jclass clazz;
    clazz = env->FindClass(JAVA_CLASS_NAME);
    if (clazz == NULL) {
        OS_LOGE(TAG, "ERROR: FindClass (%s) failed", JAVA_CLASS_NAME);
        return;
    }
    fields.mContext = env->GetFieldID(clazz, "mNativeContext", "J");
    if (fields.mContext == NULL) {
        return;
    }
    fields.mPostEvent = env->GetStaticMethodID(clazz, "postEventFromNative", "(Ljava/lang/Object;II)V");
    if (fields.mPostEvent == NULL) {
        OS_LOGE(TAG, "ERROR: Get postEvent mothod failed");
        return;
    }
    env->DeleteLocalRef(clazz);

    // Hold onto Liteplayer class for use in calling the static method
    // that posts events to the application thread.
    clazz = env->GetObjectClass(thiz);
    if (clazz == NULL) {
        OS_LOGE(TAG, "Can't find %s", JAVA_CLASS_NAME);
        jniThrowException(env, "java/lang/Exception", NULL);
        return;
    }
    fields.mClass = (jclass)env->NewGlobalRef(clazz);
    // We use a weak reference so the Liteplayer object can be garbage collected.
    // The reference is only used as a proxy for callbacks.
    fields.mObject  = env->NewGlobalRef(weak_this);

    liteplayer_handle_t p = liteplayer_create();
    if (p == NULL) {
        jniThrowException(env, "java/lang/RuntimeException", "create failed");
        return;
    }
    // Register state listener, todo: notify java objest
    liteplayer_register_state_listener(p, Liteplayer_native_stateCallback, NULL);
    // Register sink adapter
    struct sink_wrapper sink_ops = {
            .sink_priv = NULL,
            .open = opensles_wrapper_open,
            .write = opensles_wrapper_write,
            .close = opensles_wrapper_close,
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
}

static void Liteplayer_native_setDataSource(JNIEnv *env, jobject thiz, jstring path)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_setDataSource");

    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    if (path == NULL) {
        jniThrowException(env, "java/lang/IllegalArgumentException", NULL);
        return;
    }
    const char *tmp = env->GetStringUTFChars(path, NULL);
    if (tmp == NULL) {
        jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return;
    }
    std::string url = tmp;
    env->ReleaseStringUTFChars(path, tmp);
    tmp = NULL;

    if (liteplayer_set_data_source(p, url.c_str(), 0) != 0) {
        OS_LOGE(TAG, "ERROR: liteplayer_set_data_source failed");
        return;
    }
}

static void Liteplayer_native_prepareAsync(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_prepareAsync");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    if (liteplayer_prepare_async(p) != 0) {
        OS_LOGE(TAG, "ERROR: liteplayer_prepare_async failed");
        return;
    }
}

static void Liteplayer_native_start(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_start");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    if (liteplayer_start(p) != 0) {
        OS_LOGE(TAG, "ERROR: liteplayer_start failed");
        return;
    }
}

static void Liteplayer_native_pause(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_pause");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    if (liteplayer_pause(p) != 0) {
        OS_LOGE(TAG, "ERROR: liteplayer_pause failed");
        return;
    }
}

static void Liteplayer_native_resume(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_resume");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    if (liteplayer_resume(p) != 0) {
        OS_LOGE(TAG, "ERROR: liteplayer_resume failed");
        return;
    }
}

static void Liteplayer_native_seekTo(JNIEnv *env, jobject thiz, jint msec)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_seekTo");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    if (liteplayer_seek(p, msec) != 0) {
        OS_LOGE(TAG, "ERROR: liteplayer_seek failed");
        return;
    }
}

static void Liteplayer_native_stop(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_stop");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    if (liteplayer_stop(p) != 0) {
        OS_LOGE(TAG, "ERROR: liteplayer_stop failed");
        return;
    }
}

static void Liteplayer_native_reset(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_reset");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    if (liteplayer_reset(p) != 0) {
        OS_LOGE(TAG, "ERROR: liteplayer_reset failed");
        return;
    }
}

static jint Liteplayer_native_getCurrentPosition(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_getCurrentPosition");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    int msec = 0;
    liteplayer_get_position(p, &msec);
    OS_LOGD(TAG, "@@@ liteplayer_get_position: %d (msec)", msec);
    return (jint)msec;
}

static jint Liteplayer_native_getDuration(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_getCurrentPosition");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return 0;
    }
    int msec = 0;
    liteplayer_get_duration(p, &msec);
    OS_LOGD(TAG, "@@@ liteplayer_get_duration: %d (msec)", msec);
    return (jint)msec;
}

static void Liteplayer_native_destroy(JNIEnv *env, jobject thiz)
{
    OS_LOGD(TAG, "@@@ Liteplayer_native_destroy");
    liteplayer_handle_t p = getLiteplayer(env, thiz);
    if (p == NULL ) {
        jniThrowException(env, "java/lang/IllegalStateException", NULL);
        return;
    }
    setLiteplayer(env, thiz, 0);
    liteplayer_destroy(p);

    // remove global references
    env->DeleteGlobalRef(fields.mObject);
    env->DeleteGlobalRef(fields.mClass);
    fields.mObject = NULL;
    fields.mClass = NULL;
}

static jstring native_stringFromJNI(JNIEnv* env, jobject) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

static JNINativeMethod gMethods[] = {
        {"native_create", "(Ljava/lang/Object;)V", (void *)Liteplayer_native_create},
        {"native_setDataSource", "(Ljava/lang/String;)V", (void *)Liteplayer_native_setDataSource},
        {"native_prepareAsync", "()V", (void *)Liteplayer_native_prepareAsync},
        {"native_start", "()V", (void *)Liteplayer_native_start},
        {"native_pause", "()V", (void *)Liteplayer_native_pause},
        {"native_resume", "()V", (void *)Liteplayer_native_resume},
        {"native_seekTo", "(I)V", (void *)Liteplayer_native_seekTo},
        {"native_stop", "()V", (void *)Liteplayer_native_stop},
        {"native_reset", "()V", (void *)Liteplayer_native_reset},
        {"native_getCurrentPosition", "()I", (void *)Liteplayer_native_getCurrentPosition},
        {"native_getDuration", "()I", (void *)Liteplayer_native_getDuration},
        {"native_destroy", "()V", (void *)Liteplayer_native_destroy},
        {"stringFromJNI", "()Ljava/lang/String;", (void *)native_stringFromJNI},
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
        OS_LOGE(TAG, "ERROR: GetEnv failed");
        goto bail;
    }
    assert(env != NULL);

    if (registerNativeMethods(env, JAVA_CLASS_NAME, gMethods, NELEM(gMethods)) != JNI_TRUE) {
        OS_LOGE(TAG, "ERROR: registerNativeMethods failed");
        goto bail;
    }

    fields.mJavaVM = vm;
    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

    bail:
    return result;
}
