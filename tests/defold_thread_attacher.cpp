// Pinned Defold ThreadAttacher, engine 574678c7d44be490d874fbed2d0ae6211feec4d9.
// Copyright 2020-2026 The Defold Foundation; 2014-2020 King;
// 2009-2014 Ragnar Svensson, Christian Murray.
// Licensed under the Defold License 1.0: https://www.defold.com/license
// Only host adaptation: JNI desktop AttachCurrentThread takes void** rather
// than Android's JNIEnv**. The attachment/detachment behavior is unchanged.
#include <dmsdk/dlib/android.h>
namespace dmAndroid {
static android_app* g_AndroidApp = 0;
void SetAndroidApp(android_app* app) { g_AndroidApp = app; }
android_app* GetAndroidApp() { return g_AndroidApp; }
ThreadAttacher::ThreadAttacher() : m_Activity(0), m_Env(0), m_IsAttached(false) {
    android_app* app = dmAndroid::GetAndroidApp();
    if (app) {
        m_Activity = app->activity;
        if (m_Activity->vm->GetEnv((void**)&m_Env, JNI_VERSION_1_6) != JNI_OK) {
            m_Activity->vm->AttachCurrentThread((void**)&m_Env, 0);
            m_IsAttached = true;
        }
    }
}
bool ThreadAttacher::Detach() {
    bool ok = true;
    if (m_IsAttached) {
        if (m_Env->ExceptionCheck()) { m_Env->ExceptionDescribe(); ok = false; }
        m_Env->ExceptionClear(); m_Activity->vm->DetachCurrentThread(); m_IsAttached = false;
    }
    return ok;
}
}
