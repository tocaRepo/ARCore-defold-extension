#pragma once
#include <jni.h>
#include <dmsdk/dlib/log.h>
namespace dmGraphics { jobject GetNativeAndroidActivity(); }
#if defined(ARCORE_BRIDGE_TEST)
#include "bridge_sdk.h"
#endif
