// ThreadAttacher declaration adapted from Defold engine
// 574678c7d44be490d874fbed2d0ae6211feec4d9, dmsdk/dlib/android.h.
// Copyright 2020-2026 The Defold Foundation; 2014-2020 King;
// 2009-2014 Ragnar Svensson, Christian Murray.
// Licensed under the Defold License 1.0: https://www.defold.com/license
#pragma once
#include <jni.h>
struct ANativeActivity { JavaVM* vm; jobject clazz; };
struct android_app { ANativeActivity* activity; };
namespace dmAndroid {
void SetAndroidApp(android_app* app);
android_app* GetAndroidApp();
class ThreadAttacher {
    ANativeActivity* m_Activity;
    JNIEnv* m_Env;
    bool m_IsAttached;
public:
    ThreadAttacher();
    ~ThreadAttacher() { Detach(); }
    bool Detach();
    bool IsAttached() { return m_IsAttached; }
    JNIEnv* GetEnv() { return m_Env; }
};
jclass LoadClass(JNIEnv* env, const char* name);
}
