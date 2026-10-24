#if defined(DM_PLATFORM_ANDROID) || defined(ARCORE_HOST_TEST)
#include <dmsdk/sdk.h>
#include <dmsdk/dlib/android.h>
#include <dmsdk/dlib/time.h>
#include <dlfcn.h>
#include <stdio.h>
#include "ar_session.h"

namespace arcore {
Api api = {};
static void* library = 0; // Deliberately retained after successful load.

Session::Session() : session(0), frame(0), pose(0), surfaces(0), permission_class(0),
    ensure_permission(0), get_rotation(0), reset_permission(0), active(true), requested(false),
    resumed(false), fatal(false), tracking(false), geometry_matches(false), generation(0),
    update_id(0), next_plane_id(0), last_image_time(0), last_timestamp(0),
    viewport_width(0), viewport_height(0), display_rotation(0), status("stopped"), tracking_failure_reason("none") {}

void Session::Fail(const char* code, const char* message, int native_status, bool terminal) {
    status = code; detail = message; error.Set(code, message, native_status);
    tracking = false; fatal = terminal;
}
bool Session::CheckJNI(JNIEnv* env) {
    if (!env->ExceptionCheck()) return true;
    env->ExceptionDescribe(); env->ExceptionClear();
    Fail("android_error", "Android bridge failed; see logcat and restart the session.", 0, true);
    return false;
}
bool Session::LoadFailed(const char* reason) {
    dmLogError("ARCore SDK load failed: %s", reason);
    Fail("sdk_load_failed", reason, 0, true); return false;
}
bool Session::LoadApi(JNIEnv* env) {
    if (library) return true;
    jclass loader = dmAndroid::LoadClass(env, "com.defold.arcore.ArCoreLoader");
    if (!CheckJNI(env) || !loader) return LoadFailed("Could not find the application's ARCore loader.");
    jmethodID load = env->GetStaticMethodID(loader, "load", "()Ljava/lang/String;");
    if (!CheckJNI(env) || !load) { env->DeleteLocalRef(loader); return LoadFailed("Could not find ArCoreLoader.load."); }
    jstring path = (jstring)env->CallStaticObjectMethod(loader, load);
    env->DeleteLocalRef(loader);
    if (!CheckJNI(env) || !path) return LoadFailed("Android could not load ARCore; see the Java exception.");
    const char* filename = env->GetStringUTFChars(path, 0);
    if (!filename) { CheckJNI(env); env->DeleteLocalRef(path); return LoadFailed("Could not read ARCore's resolved path."); }
    void* candidate = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
    if (!candidate) { const char* e = dlerror(); dmLogError("dlopen %s: %s", filename, e ? e : "unknown error"); }
    env->ReleaseStringUTFChars(path, filename); env->DeleteLocalRef(path);
    if (!candidate) return LoadFailed("Could not open the class-loader-resolved ARCore path.");
    Api loaded = {};
#define AR_LOAD(n) \
    loaded.n = reinterpret_cast<decltype(loaded.n)>(dlsym(candidate, #n)); \
    if (!loaded.n) { dmLogError("ARCore export missing: %s", #n); dlclose(candidate); return LoadFailed("Packaged ARCore SDK has an incompatible API."); }
    AR_FUNCTIONS(AR_LOAD)
#undef AR_LOAD
    api = loaded; library = candidate;
    dmLogInfo("ARCore SDK loaded through application class loader; all C exports resolved (arm64-v8a)");
    return true;
}

void Session::Start(JNIEnv* env, const Options& next_options) {
    Stop();
    ++generation; next_plane_id = 0; update_id = 0;
    anchors.Reset(generation); options = next_options;
    requested = true; fatal = false; status = "starting"; detail.clear(); error = Error();
    capabilities = Capabilities(); capabilities.generation = generation;
    // Explicit start retries a denied dialog. Ordinary polls never nag after denial.
    if (permission_class && reset_permission) { env->CallStaticVoidMethod(permission_class, reset_permission); CheckJNI(env); }
}
void Session::Stop() {
    tracking = false; requested = false;
    anchors.Each([this](const std::string&, AnchorEntry& e) { api.ArAnchor_detach(session, e.anchor); api.ArAnchor_release(e.anchor); });
    anchors.Reset(generation);
    // Retained references protect stable IDs from native pointer reuse. Stopped
    // planes and their tombstones live until this session is destroyed.
    for (size_t i = 0; i < planes.size(); ++i) api.ArTrackable_release(planes[i].trackable);
    planes.clear();
    if (session && resumed) api.ArSession_pause(session);
    resumed = false;
    if (surfaces) api.ArTrackableList_destroy(surfaces);
    if (pose) api.ArPose_destroy(pose);
    if (frame) api.ArFrame_destroy(frame);
    if (session) api.ArSession_destroy(session);
    session = 0; frame = 0; pose = 0; surfaces = 0;
    last_timestamp = 0; last_image_time = 0; geometry_matches = false;
    snapshot = FrameSnapshot(); status = "stopped"; detail.clear(); error = Error();
    capabilities = Capabilities(); capabilities.generation = generation;
}
void Session::Finalize(JNIEnv* env) {
    Stop();
    if (permission_class) env->DeleteGlobalRef(permission_class);
    permission_class = 0; ensure_permission = 0; get_rotation = 0; reset_permission = 0;
    dmLogInfo("ARCore objects released before worker JVM detach");
}
void Session::SetActive(bool value) {
    active = value; tracking = false; last_image_time = 0; geometry_matches = false;
    if (!value && session && resumed) { api.ArSession_pause(session); resumed = false; }
    // Preserve last_timestamp: only a genuinely new image can reactivate input.
}
void Session::GetCapabilities(Capabilities& out) const {
    out = capabilities; out.status = active ? status : "paused"; out.error = error;
    out.generation = generation; out.sdk_loaded = library != 0;
    out.initialized = session && !fatal;
}

bool Session::EnsureSession(JNIEnv* env, int width, int height) {
    if (!requested || !active || fatal) return false;
    jobject activity = dmGraphics::GetNativeAndroidActivity();
    if (!activity) { status = "waiting_for_activity"; return false; }
    if (!LoadApi(env)) return false;
    if (!permission_class) {
        jclass local = dmAndroid::LoadClass(env, "com.defold.arcore.CameraPermission");
        if (!CheckJNI(env) || !local) { if (!fatal) Fail("android_error", "Camera permission bridge is absent.", 0, true); return false; }
        permission_class = (jclass)env->NewGlobalRef(local); env->DeleteLocalRef(local);
        if (!CheckJNI(env) || !permission_class) return false;
        ensure_permission = env->GetStaticMethodID(permission_class, "ensure", "(Landroid/app/Activity;)I");
        if (!CheckJNI(env)) return false;
        get_rotation = env->GetStaticMethodID(permission_class, "rotation", "(Landroid/app/Activity;)I");
        if (!CheckJNI(env)) return false;
        reset_permission = env->GetStaticMethodID(permission_class, "retry", "()V");
        if (!CheckJNI(env)) return false;
    }
    int permission = env->CallStaticIntMethod(permission_class, ensure_permission, activity);
    if (!CheckJNI(env)) return false;
    if (permission != 1) {
        Fail(permission < 0 ? "permission_denied" : "permission_required",
            permission < 0 ? "Camera permission denied. Grant it in Settings or restart to retry." : "Waiting for camera permission.");
        if (session && resumed) { api.ArSession_pause(session); resumed = false; }
        last_image_time = 0; return false;
    }
    if (!session) {
        ArAvailability availability = AR_AVAILABILITY_UNKNOWN_ERROR;
        api.ArCoreApk_checkAvailability(env, activity, &availability);
        if (!CheckJNI(env)) return false;
        switch (availability) {
        case AR_AVAILABILITY_SUPPORTED_INSTALLED: capabilities.availability = "installed"; break;
        case AR_AVAILABILITY_UNKNOWN_CHECKING: capabilities.availability = "checking"; status = "checking_arcore"; detail = "Checking installed ARCore services."; error = Error(); return false;
        case AR_AVAILABILITY_UNSUPPORTED_DEVICE_NOT_CAPABLE: capabilities.availability = "unsupported"; Fail("unsupported_device", "ARCore reports that this device is unsupported.", availability, true); return false;
        case AR_AVAILABILITY_SUPPORTED_NOT_INSTALLED: capabilities.availability = "not_installed"; Fail("arcore_required", "Install Google Play Services for AR before offline use.", availability, true); return false;
        case AR_AVAILABILITY_SUPPORTED_APK_TOO_OLD: capabilities.availability = "outdated"; Fail("arcore_required", "Update Google Play Services for AR before offline use.", availability, true); return false;
        default: capabilities.availability = "unknown"; Fail("availability_unknown", "ARCore availability is unknown; retry when services are ready.", availability); return false;
        }
        ArStatus result = api.ArSession_create(env, activity, &session);
        if (!CheckJNI(env)) return false;
        if (result != AR_SUCCESS || !session) {
            session = 0;
            const char* code = result == AR_UNAVAILABLE_DEVICE_NOT_COMPATIBLE ? "unsupported_device" :
                (result == AR_UNAVAILABLE_ARCORE_NOT_INSTALLED || result == AR_UNAVAILABLE_APK_TOO_OLD) ? "arcore_required" :
                result == AR_ERROR_FATAL ? "arcore_service_failed" : "session_failed";
            Fail(code, "ARCore session creation failed; see native_status and logcat.", result, true); return false;
        }
        if (!EnumerateCameras()) return false;
        ArConfig* config = 0;
        api.ArConfig_create(session, &config);
        api.ArConfig_setPlaneFindingMode(session, config, AR_PLANE_FINDING_MODE_HORIZONTAL_AND_VERTICAL);
        api.ArConfig_setLightEstimationMode(session, config, AR_LIGHT_ESTIMATION_MODE_DISABLED);
        api.ArConfig_setDepthMode(session, config, AR_DEPTH_MODE_DISABLED);
        api.ArConfig_setFocusMode(session, config, AR_FOCUS_MODE_AUTO);
        api.ArConfig_setUpdateMode(session, config, AR_UPDATE_MODE_LATEST_CAMERA_IMAGE);
        api.ArConfig_setTextureUpdateMode(session, config, AR_TEXTURE_UPDATE_MODE_EXPOSE_HARDWARE_BUFFER);
        result = api.ArSession_configure(session, config); api.ArConfig_destroy(config);
        if (result != AR_SUCCESS) { Fail("configuration_failed", "ARCore configuration failed (Android API 27+ required).", result, true); return false; }
        int32_t depth = 0; api.ArSession_isDepthModeSupported(session, AR_DEPTH_MODE_AUTOMATIC, &depth);
        capabilities.depth_supported = depth ? 1 : 0;
        api.ArFrame_create(session, &frame); api.ArPose_create(session, 0, &pose); api.ArTrackableList_create(session, &surfaces);
    }
    int rotation = env->CallStaticIntMethod(permission_class, get_rotation, activity);
    if (!CheckJNI(env)) return false;
    viewport_width = width; viewport_height = height; display_rotation = rotation;
    geometry_matches = snapshot.complete && snapshot.viewport_width == width &&
        snapshot.viewport_height == height && snapshot.display_rotation == rotation;
    api.ArSession_setDisplayGeometry(session, rotation, width, height);
    if (!resumed) {
        ArStatus result = api.ArSession_resume(session);
        if (!CheckJNI(env)) return false;
        if (result != AR_SUCCESS) { Fail("camera_unavailable", "ARCore camera cannot resume; close other camera apps and retry.", result); return false; }
        resumed = true; dmLogInfo("ARCore session resumed (dedicated worker)");
    }
    error = Error(); detail.clear(); return true;
}

CameraConfig Session::ReadCameraConfig(ArCameraConfig* config) {
    CameraConfig out;
    char* id = 0; api.ArCameraConfig_getCameraId(session, config, &id);
    if (id) { out.camera_id = id; api.ArString_release(id); }
    api.ArCameraConfig_getImageDimensions(session, config, &out.image_width, &out.image_height);
    api.ArCameraConfig_getTextureDimensions(session, config, &out.texture_width, &out.texture_height);
    api.ArCameraConfig_getFpsRange(session, config, &out.min_fps, &out.max_fps);
    api.ArCameraConfig_getDepthSensorUsage(session, config, &out.depth_sensor_usage);
    ArCameraConfigStereoCameraUsage stereo; api.ArCameraConfig_getStereoCameraUsage(session, config, &stereo);
    out.stereo_camera_usage = (uint32_t)stereo;
    ArCameraConfigFacingDirection facing; api.ArCameraConfig_getFacingDirection(session, config, &facing);
    out.facing = facing == AR_CAMERA_CONFIG_FACING_DIRECTION_BACK ? "back" : "front";
    char suffix[180]; snprintf(suffix, sizeof(suffix), ":%dx%d:%dx%d:%d-%d:%u:%u", out.image_width, out.image_height,
        out.texture_width, out.texture_height, out.min_fps, out.max_fps, out.depth_sensor_usage, out.stereo_camera_usage);
    out.id = out.facing + ":" + out.camera_id + suffix;
    return out;
}
bool Session::EnumerateCameras() {
    ArCameraConfig* current = 0; api.ArCameraConfig_create(session, &current);
    api.ArSession_getCameraConfig(session, current);
    CameraConfig default_config = ReadCameraConfig(current);
    capabilities.selected_camera_config_id = default_config.id;
    ArCameraConfigFilter* filter = 0; ArCameraConfigList* list = 0;
    api.ArCameraConfigFilter_create(session, &filter); api.ArCameraConfigList_create(session, &list);
    api.ArCameraConfigFilter_setFacingDirection(session, filter, AR_CAMERA_CONFIG_FACING_DIRECTION_BACK);
    api.ArCameraConfigFilter_setTargetFps(session, filter, AR_CAMERA_CONFIG_TARGET_FPS_30 | AR_CAMERA_CONFIG_TARGET_FPS_60);
    api.ArSession_getSupportedCameraConfigsWithFilter(session, filter, list);
    int32_t count = 0; api.ArCameraConfigList_getSize(session, list, &count);
    capabilities.cameras.clear(); bool found = options.camera_config_id.empty(); ArStatus result = AR_SUCCESS;
    for (int i = 0; i < count; ++i) {
        api.ArCameraConfigList_getItem(session, list, i, current);
        CameraConfig c = ReadCameraConfig(current);
        if (!options.camera_config_id.empty() && c.id == options.camera_config_id && !found) {
            result = api.ArSession_setCameraConfig(session, current); found = true;
            if (result == AR_SUCCESS) capabilities.selected_camera_config_id = c.id;
        }
        capabilities.cameras.push_back(c);
    }
    // Never change the default camera when no explicit alternative was requested.
    api.ArCameraConfig_destroy(current); api.ArCameraConfigList_destroy(list); api.ArCameraConfigFilter_destroy(filter);
    for (size_t i = 0; i < capabilities.cameras.size(); ++i)
        capabilities.cameras[i].selected = capabilities.cameras[i].id == capabilities.selected_camera_config_id;
    if (!found) { Fail("invalid_camera_config", "Requested camera configuration is not exposed by ARCore on this device.", 0, true); return false; }
    if (result != AR_SUCCESS) { Fail("camera_config_failed", "ARCore rejected the requested camera configuration.", result, true); return false; }
    return true;
}
}
#endif
