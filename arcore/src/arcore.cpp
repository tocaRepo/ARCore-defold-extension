#if defined(DM_PLATFORM_ANDROID) && !defined(__aarch64__)
#error "ARCore requires ARM64 Android. Select arm64-android and disable armv7-android."
#endif
#include <dmsdk/sdk.h>
#include <cmath>
#include <string.h>
#include "ar_types.h"

using namespace arcore;
static void String(lua_State* L, const char* key, const std::string& v) { lua_pushlstring(L, v.c_str(), v.size()); lua_setfield(L, -2, key); }
static void Boolean(lua_State* L, const char* key, bool v) { lua_pushboolean(L, v); lua_setfield(L, -2, key); }
static void Number(lua_State* L, const char* key, double v) { lua_pushnumber(L, v); lua_setfield(L, -2, key); }
static void PushError(lua_State* L, const Error& e) {
    lua_newtable(L); String(L, "code", e.code); String(L, "message", e.message); Number(L, "native_status", e.native_status);
}
static int Failure(lua_State* L, const Error& e) { lua_pushnil(L); PushError(L, e); return 2; }
static int Failure(lua_State* L, const char* code, const char* message) { Error e; e.Set(code, message); return Failure(L, e); }
static bool Dimension(lua_State* L, int index, int& out) {
    double v = luaL_checknumber(L, index);
    if (!std::isfinite(v) || v <= 0 || v > 32768 || v != floor(v)) return false;
    out = (int)v; return true;
}
static bool HasOnly(lua_State* L, int index, const char* const* keys, Error& error) {
    lua_pushnil(L);
    while (lua_next(L, index)) {
        bool known = false;
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char* key = lua_tostring(L, -2);
            for (int i = 0; keys[i]; ++i) if (!strcmp(keys[i], key)) known = true;
        }
        lua_pop(L, 1);
        if (!known) { lua_pop(L, 1); error.Set("unsupported_option", "Options contain an unknown or unsupported field."); return false; }
    }
    return true;
}
static double OptionalNumber(lua_State* L, int index, const char* key, double fallback) {
    lua_getfield(L, index, key); double value = lua_isnil(L, -1) ? fallback : luaL_checknumber(L, -1); lua_pop(L, 1); return value;
}
static std::string OptionalString(lua_State* L, int index, const char* key) {
    lua_getfield(L, index, key);
    if (!lua_isnil(L, -1)) luaL_checktype(L, -1, LUA_TSTRING);
    std::string value = lua_isnil(L, -1) ? "" : lua_tostring(L, -1); lua_pop(L, 1); return value;
}
static uint64_t OptionalIdentifier(lua_State* L, int index, const char* key) {
    double v = OptionalNumber(L, index, key, 0);
    if (!std::isfinite(v) || v < 0 || v > 9007199254740991.0 || v != floor(v)) luaL_error(L, "%s must be a nonnegative exact integer", key);
    return (uint64_t)v;
}

#if defined(DM_PLATFORM_ANDROID)
#include <dmsdk/dlib/time.h>
#include "ar_session.h"
#include "ar_worker.h"
static ArWorker worker;
static Session session;
static FrameSnapshot snapshot;
static dmBuffer::HBuffer pixels = 0;
static int pixels_ref = LUA_NOREF, pixels_width = 0, pixels_height = 0;
static uint64_t pixels_frame_id = 0;
struct PollRequest { int width, height; FrameSnapshot* snapshot; };
static void StartJob(JNIEnv* env, void* p) { session.Start(env, *static_cast<Options*>(p)); }
static void StopJob(JNIEnv*, void*) { session.Stop(); }
static void PollJob(JNIEnv* env, void* p) { PollRequest& r = *static_cast<PollRequest*>(p); session.Poll(env, r.width, r.height, *r.snapshot); }
static void CapabilitiesJob(JNIEnv*, void* p) { session.GetCapabilities(*static_cast<Capabilities*>(p)); }
static void HitJob(JNIEnv*, void* p) { session.HitTest(*static_cast<HitRequest*>(p)); }
static void CreateJob(JNIEnv*, void* p) { session.CreateAnchor(*static_cast<AnchorRequest*>(p)); }
static void ReleaseJob(JNIEnv*, void* p) { session.ReleaseAnchor(*static_cast<ReleaseRequest*>(p)); }
static void ActiveJob(JNIEnv*, void* p) { session.SetActive(*static_cast<bool*>(p)); }
static void FinalizeJob(JNIEnv* env, void*) { session.Finalize(env); }

static void ClearPixels(lua_State* L) {
    if (pixels_ref != LUA_NOREF) dmScript::Unref(L, LUA_REGISTRYINDEX, pixels_ref);
    pixels_ref = LUA_NOREF; pixels = 0; pixels_width = pixels_height = 0; pixels_frame_id = 0; snapshot = FrameSnapshot();
}
static bool CopyPixels(lua_State* L) {
    int w = snapshot.width, h = snapshot.height;
    if (!pixels || w != pixels_width || h != pixels_height) {
        if (pixels_ref != LUA_NOREF) dmScript::Unref(L, LUA_REGISTRYINDEX, pixels_ref);
        pixels_ref = LUA_NOREF; pixels = 0;
        dmBuffer::StreamDeclaration stream = {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT8, 3};
        if (dmBuffer::Create(w*h, &stream, 1, &pixels) != dmBuffer::RESULT_OK) return false;
        dmScript::LuaHBuffer buffer(pixels, dmScript::OWNER_LUA); dmScript::PushBuffer(L, buffer);
        pixels_ref = dmScript::Ref(L, LUA_REGISTRYINDEX); pixels_width = w; pixels_height = h;
    }
    void* out = 0; uint32_t size = 0;
    if (dmBuffer::GetBytes(pixels, &out, &size) != dmBuffer::RESULT_OK || size < snapshot.rgb.size()) return false;
    memcpy(out, snapshot.rgb.data(), snapshot.rgb.size()); dmBuffer::UpdateContentVersion(pixels); return true;
}
static void Matrix(lua_State* L, const char* key, const float* v) {
    dmVMath::Matrix4 m;
    for (int c = 0; c < 4; ++c) m.setCol(c, dmVMath::Vector4(v[c*4], v[c*4+1], v[c*4+2], v[c*4+3]));
    dmScript::PushMatrix4(L, m); lua_setfield(L, -2, key);
}
static void Position(lua_State* L, const char* key, const Pose& p) { dmScript::PushVector3(L, dmVMath::Vector3(p.raw[4], p.raw[5], p.raw[6])); lua_setfield(L, -2, key); }
static void Rotation(lua_State* L, const char* key, const Pose& p) { dmScript::PushQuat(L, dmVMath::Quat(p.raw[0], p.raw[1], p.raw[2], p.raw[3])); lua_setfield(L, -2, key); }
static void PushPose(lua_State* L, const Pose& p) { lua_newtable(L); Position(L, "position", p); Rotation(L, "rotation", p); }
static void PoseField(lua_State* L, const Pose& p) { PushPose(L, p); lua_setfield(L, -2, "pose"); }
static void FrameIdentifiers(lua_State* L, uint64_t id, uint64_t generation) { Number(L, "frame_id", (double)id); Number(L, "session_generation", (double)generation); }

static int Start(lua_State* L) {
    Options options; Error error;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        const char* const keys[] = {"near_clip", "far_clip", "camera_config_id", 0};
        if (!HasOnly(L, 1, keys, error)) return Failure(L, error);
        double near_clip = OptionalNumber(L, 1, "near_clip", options.near_clip);
        double far_clip = OptionalNumber(L, 1, "far_clip", options.far_clip);
        if (!std::isfinite(near_clip) || !std::isfinite(far_clip) || near_clip <= 0 || far_clip <= near_clip || far_clip > 100000)
            return Failure(L, "invalid_options", "Clipping requires 0 < near_clip < far_clip <= 100000 metres.");
        options.near_clip = (float)near_clip; options.far_clip = (float)far_clip;
        if (options.near_clip <= 0 || options.far_clip <= options.near_clip)
            return Failure(L, "invalid_options", "Clipping values must remain distinct positive float values.");
        options.camera_config_id = OptionalString(L, 1, "camera_config_id");
    }
    if (!worker.Invoke(StartJob, &options)) return Failure(L, "worker_unavailable", "Could not attach the ARCore worker to Java.");
    ClearPixels(L); lua_pushboolean(L, true); return 1;
}
static int Shutdown(lua_State* L) {
    if (!worker.Invoke(StopJob, 0)) return Failure(L, "worker_unavailable", "Could not run ARCore cleanup on its worker.");
    ClearPixels(L); lua_pushboolean(L, true); return 1;
}
static int Poll(lua_State* L) {
    int width = 0, height = 0;
    if (!Dimension(L, 1, width) || !Dimension(L, 2, height)) return Failure(L, "invalid_viewport", "poll requires positive integer physical viewport dimensions (up to 32768).");
    PollRequest request = {width, height, &snapshot};
    if (!worker.Invoke(PollJob, &request)) return Failure(L, "worker_unavailable", "Could not attach the ARCore worker to Java.");
    if (snapshot.complete && snapshot.frame_id != pixels_frame_id) {
        if (!CopyPixels(L)) return Failure(L, "buffer_unavailable", "Could not copy the complete camera image to a Defold buffer.");
        pixels_frame_id = snapshot.frame_id;
        snapshot.fresh = true; // Also covers retry after a prior engine-buffer allocation failure.
    }
    lua_newtable(L); Boolean(L, "native", true); String(L, "status", snapshot.status); String(L, "detail", snapshot.detail);
    Boolean(L, "tracking", snapshot.tracking); Boolean(L, "fresh", snapshot.fresh); Boolean(L, "complete", snapshot.complete);
    FrameIdentifiers(L, snapshot.frame_id, snapshot.session_generation); String(L, "tracking_failure_reason", snapshot.tracking_failure_reason);
    if (!snapshot.error.Ok()) { PushError(L, snapshot.error); lua_setfield(L, -2, "error"); }
    lua_newtable(L);
    for (size_t i = 0; i < snapshot.planes.size(); ++i) {
        const PlaneSnapshot& p = snapshot.planes[i]; lua_newtable(L);
        String(L, "id", p.id); String(L, "tracking_state", p.state); String(L, "orientation", p.orientation);
        if (!p.parent_id.empty()) String(L, "parent_id", p.parent_id);
        PoseField(L, p.pose); Number(L, "extent_x", p.extent_x); Number(L, "extent_z", p.extent_z);
        FrameIdentifiers(L, snapshot.frame_id, snapshot.session_generation);
        lua_newtable(L);
        for (size_t j = 0; j < p.polygon.size(); ++j) { lua_pushnumber(L, p.polygon[j]); lua_rawseti(L, -2, (int)j+1); }
        lua_setfield(L, -2, "polygon"); lua_rawseti(L, -2, (int)i+1);
    }
    lua_setfield(L, -2, "planes");
    lua_newtable(L);
    for (size_t i = 0; i < snapshot.anchors.size(); ++i) {
        const AnchorSnapshot& a = snapshot.anchors[i]; lua_newtable(L);
        String(L, "handle", a.handle); String(L, "tracking_state", a.state); PoseField(L, a.pose);
        if (!a.plane_id.empty()) String(L, "plane_id", a.plane_id);
        FrameIdentifiers(L, snapshot.frame_id, snapshot.session_generation); lua_rawseti(L, -2, (int)i+1);
    }
    lua_setfield(L, -2, "anchors");
    if (snapshot.complete && pixels_ref != LUA_NOREF) {
        Matrix(L, "view", snapshot.view); Matrix(L, "projection", snapshot.projection);
        Position(L, "camera_position", snapshot.camera_pose); Rotation(L, "camera_rotation", snapshot.camera_pose);
        for (int i = 0; i < 3; ++i) {
            char key[] = "uv0"; key[2] += i;
            dmScript::PushVector4(L, dmVMath::Vector4(snapshot.uv[i*2], snapshot.uv[i*2+1], 0, 0)); lua_setfield(L, -2, key);
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, pixels_ref); lua_setfield(L, -2, "pixels");
        Number(L, "width", snapshot.width); Number(L, "height", snapshot.height); Number(L, "timestamp", (double)snapshot.timestamp);
        Number(L, "viewport_width", snapshot.viewport_width); Number(L, "viewport_height", snapshot.viewport_height); Number(L, "display_rotation", snapshot.display_rotation);
    }
    return 1;
}
static int HitTest(lua_State* L) {
    HitRequest r; double x = luaL_checknumber(L, 1), y = luaL_checknumber(L, 2);
    if (!std::isfinite(x) || !std::isfinite(y) || fabs(x) > 32768 || fabs(y) > 32768) return Failure(L, "invalid_coordinates", "Hit coordinates must be finite viewport pixels.");
    r.x = (float)x; r.y = (float)y;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE); const char* const keys[] = {"frame_id", "session_generation", 0};
        if (!HasOnly(L, 3, keys, r.error)) return Failure(L, r.error);
        r.expected_frame = OptionalIdentifier(L, 3, "frame_id"); r.expected_generation = OptionalIdentifier(L, 3, "session_generation");
    }
    if (!worker.Invoke(HitJob, &r)) return Failure(L, "worker_unavailable", "ARCore worker is unavailable.");
    if (!r.error.Ok()) return Failure(L, r.error);
    lua_newtable(L);
    for (size_t i = 0; i < r.hits.size(); ++i) {
        const Hit& h = r.hits[i]; lua_newtable(L); PoseField(L, h.pose); Number(L, "distance", h.distance); String(L, "plane_id", h.plane_id);
        FrameIdentifiers(L, h.frame_id, h.generation); lua_rawseti(L, -2, (int)i+1);
    }
    return 1;
}
static int CreateAnchor(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE); AnchorRequest r;
    const char* const keys[] = {"pose", "plane_id", "frame_id", "session_generation", 0};
    if (!HasOnly(L, 1, keys, r.error)) return Failure(L, r.error);
    r.plane_id = OptionalString(L, 1, "plane_id"); r.expected_frame = OptionalIdentifier(L, 1, "frame_id"); r.expected_generation = OptionalIdentifier(L, 1, "session_generation");
    lua_getfield(L, 1, "plane_id"); bool attached = !lua_isnil(L, -1); lua_pop(L, 1);
    if (attached && r.plane_id.empty()) return Failure(L, "invalid_plane", "An attached anchor requires a nonempty plane ID.");
    lua_getfield(L, 1, "pose"); luaL_checktype(L, -1, LUA_TTABLE);
    lua_getfield(L, -1, "position"); const dmVMath::Vector3* p = dmScript::CheckVector3(L, -1);
    r.pose.raw[4] = p->getX(); r.pose.raw[5] = p->getY(); r.pose.raw[6] = p->getZ(); lua_pop(L, 1);
    lua_getfield(L, -1, "rotation");
    if (!lua_isnil(L, -1)) {
        const dmVMath::Quat* q = dmScript::CheckQuat(L, -1);
        r.pose.raw[0] = q->getX(); r.pose.raw[1] = q->getY(); r.pose.raw[2] = q->getZ(); r.pose.raw[3] = q->getW();
    }
    lua_pop(L, 2);
    for (int i = 0; i < 7; ++i) if (!std::isfinite(r.pose.raw[i])) return Failure(L, "invalid_pose", "Pose components must be finite.");
    double length = 0; for (int i = 0; i < 4; ++i) length += (double)r.pose.raw[i]*r.pose.raw[i];
    if (length < 1e-12) return Failure(L, "invalid_pose", "Pose quaternion must be nonzero.");
    length = sqrt(length); for (int i = 0; i < 4; ++i) r.pose.raw[i] = (float)(r.pose.raw[i] / length);
    if (!worker.Invoke(CreateJob, &r)) return Failure(L, "worker_unavailable", "ARCore worker is unavailable.");
    if (!r.error.Ok()) return Failure(L, r.error);
    lua_pushlstring(L, r.handle.c_str(), r.handle.size()); return 1;
}
static int ReleaseAnchor(lua_State* L) {
    luaL_checktype(L, 1, LUA_TSTRING); ReleaseRequest r; r.handle = lua_tostring(L, 1);
    if (!worker.Invoke(ReleaseJob, &r)) return Failure(L, "worker_unavailable", "ARCore worker is unavailable.");
    if (!r.error.Ok()) return Failure(L, r.error);
    lua_pushboolean(L, true); return 1;
}
static void OnEvent(dmExtension::Params*, const dmExtension::Event* e) {
    if (e->m_Event != dmExtension::EVENT_ID_ACTIVATEAPP && e->m_Event != dmExtension::EVENT_ID_DEACTIVATEAPP) return;
    bool active = e->m_Event == dmExtension::EVENT_ID_ACTIVATEAPP; worker.Invoke(ActiveJob, &active);
}
#else
static int Unsupported(lua_State* L) { return Failure(L, "unsupported_platform", "ARCore requires an ARM64 Android device; this platform has no camera or simulator."); }
static int Start(lua_State* L) { return Unsupported(L); }
static int Shutdown(lua_State* L) { lua_pushboolean(L, true); return 1; }
static int HitTest(lua_State* L) { return Unsupported(L); }
static int CreateAnchor(lua_State* L) { return Unsupported(L); }
static int ReleaseAnchor(lua_State* L) { return Unsupported(L); }
static int Poll(lua_State* L) {
    int w, h;
    if (!Dimension(L, 1, w) || !Dimension(L, 2, h)) return Failure(L, "invalid_viewport", "poll requires positive integer physical viewport dimensions.");
    lua_newtable(L); Boolean(L, "native", false); String(L, "status", "unsupported_platform");
    String(L, "detail", "ARCore requires ARM64 Android; no simulated camera is provided.");
    Boolean(L, "tracking", false); Boolean(L, "fresh", false); Boolean(L, "complete", false);
    Number(L, "frame_id", 0); Number(L, "session_generation", 0); String(L, "tracking_failure_reason", "none");
    Error e; e.Set("unsupported_platform", "ARCore requires ARM64 Android."); PushError(L, e); lua_setfield(L, -2, "error");
    lua_newtable(L); lua_setfield(L, -2, "planes"); lua_newtable(L); lua_setfield(L, -2, "anchors"); return 1;
}
static void OnEvent(dmExtension::Params*, const dmExtension::Event*) {}
#endif

static int GetCapabilities(lua_State* L) {
    Capabilities c;
#if defined(DM_PLATFORM_ANDROID)
    if (!worker.Invoke(CapabilitiesJob, &c)) { c.status = "worker_unavailable"; c.error.Set("worker_unavailable", "ARCore worker cannot attach to Java."); }
    const bool native = true;
#else
    c.status = "unsupported_platform"; const bool native = false;
#endif
    lua_newtable(L); Boolean(L, "native", native); Boolean(L, "initialized", c.initialized);
    String(L, "status", c.status); Number(L, "session_generation", (double)c.generation); Number(L, "max_anchors", MAX_ANCHORS);
    if (!c.error.Ok()) { PushError(L, c.error); lua_setfield(L, -2, "error"); }
    lua_newtable(L);
    const char* implemented[] = {"camera_rgb", "camera_transforms", "planes", "hit_test", "anchors", "tracking_diagnostics", "camera_config_selection", 0};
    const char* future[] = {"depth", "occlusion", "augmented_images", "light_estimation", "front_camera", "cloud_anchors", "geospatial", "ios", "multiplayer", 0};
    for (int i = 0; implemented[i]; ++i) Boolean(L, implemented[i], true);
    for (int i = 0; future[i]; ++i) Boolean(L, future[i], false);
    lua_setfield(L, -2, "implementation");
    lua_newtable(L); String(L, "sdk_version", "1.48.0"); Boolean(L, "sdk_loaded", c.sdk_loaded); String(L, "availability", c.availability);
    String(L, "depth", c.depth_supported < 0 ? "unknown" : c.depth_supported ? "supported" : "unsupported");
    String(L, "rear_camera", c.initialized ? "supported" : "unknown"); String(L, "front_camera", "unknown");
    String(L, "selected_camera_config_id", c.selected_camera_config_id); lua_setfield(L, -2, "runtime");
    lua_newtable(L);
    for (size_t i = 0; i < c.cameras.size(); ++i) {
        const CameraConfig& camera = c.cameras[i]; lua_newtable(L);
        String(L, "id", camera.id); String(L, "camera_id", camera.camera_id); String(L, "facing", camera.facing);
        Number(L, "image_width", camera.image_width); Number(L, "image_height", camera.image_height);
        Number(L, "texture_width", camera.texture_width); Number(L, "texture_height", camera.texture_height);
        Number(L, "min_fps", camera.min_fps); Number(L, "max_fps", camera.max_fps);
        Number(L, "depth_sensor_usage", camera.depth_sensor_usage); Number(L, "stereo_camera_usage", camera.stereo_camera_usage);
        Boolean(L, "selected", camera.selected); lua_rawseti(L, -2, (int)i+1);
    }
    lua_setfield(L, -2, "camera_configs"); return 1;
}
static const luaL_reg methods[] = {
    {"start", Start}, {"poll", Poll}, {"get_capabilities", GetCapabilities},
    {"hit_test", HitTest}, {"create_anchor", CreateAnchor}, {"release_anchor", ReleaseAnchor}, {"stop", Shutdown}, {0, 0}
};
static dmExtension::Result Initialize(dmExtension::Params* p) {
    luaL_register(p->m_L, "arcore", methods); lua_pop(p->m_L, 1); dmLogInfo("ARCore experimental extension initialized"); return dmExtension::RESULT_OK;
}
static dmExtension::Result Finalize(dmExtension::Params* p) {
#if defined(DM_PLATFORM_ANDROID)
    worker.Close(FinalizeJob); ClearPixels(p->m_L);
#endif
    return dmExtension::RESULT_OK;
}
DM_DECLARE_EXTENSION(ARCore, "ARCore", 0, 0, Initialize, 0, OnEvent, Finalize)
