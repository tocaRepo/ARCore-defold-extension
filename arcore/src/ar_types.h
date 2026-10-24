#ifndef DEFOLD_ARCORE_TYPES_H
#define DEFOLD_ARCORE_TYPES_H

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace arcore {
static const size_t MAX_ANCHORS = 16;
static const uint64_t IMAGE_STALL_US = 250000;

struct Error {
    std::string code, message;
    int native_status;
    Error() : native_status(0) {}
    bool Ok() const { return code.empty(); }
    void Set(const char* c, const char* m, int s = 0) { code = c; message = m; native_status = s; }
};
struct Options {
    float near_clip, far_clip;
    std::string camera_config_id;
    Options() : near_clip(0.05f), far_clip(50.0f) {}
};
struct Pose {
    // ARCore raw order: quaternion xyzw, position xyz. Metres, right handed.
    float raw[7];
    Pose() { for (int i = 0; i < 7; ++i) raw[i] = i == 3 ? 1.0f : 0.0f; }
};
struct PlaneSnapshot {
    std::string id, parent_id, state, orientation;
    Pose pose;
    float extent_x, extent_z;
    std::vector<float> polygon;
    PlaneSnapshot() : extent_x(0), extent_z(0) {}
};
struct AnchorSnapshot {
    std::string handle, plane_id, state;
    Pose pose;
};
struct FrameSnapshot {
    std::string status, detail, tracking_failure_reason;
    Error error;
    bool tracking, fresh, complete;
    uint64_t frame_id, session_generation;
    int64_t timestamp;
    int width, height, viewport_width, viewport_height, display_rotation;
    float view[16], projection[16], uv[6];
    Pose camera_pose;
    std::vector<uint8_t> rgb;
    std::vector<PlaneSnapshot> planes;
    std::vector<AnchorSnapshot> anchors;
    FrameSnapshot() : status("stopped"), tracking_failure_reason("none"),
        tracking(false), fresh(false), complete(false), frame_id(0), session_generation(0),
        timestamp(0), width(0), height(0), viewport_width(0), viewport_height(0), display_rotation(0) {
        for (int i = 0; i < 16; ++i) view[i] = projection[i] = i % 5 == 0 ? 1.0f : 0.0f;
        for (int i = 0; i < 6; ++i) uv[i] = 0;
    }
};
struct CameraConfig {
    std::string id, camera_id, facing;
    int image_width, image_height, texture_width, texture_height, min_fps, max_fps;
    uint32_t depth_sensor_usage, stereo_camera_usage;
    bool selected;
    CameraConfig() : image_width(0), image_height(0), texture_width(0), texture_height(0),
        min_fps(0), max_fps(0), depth_sensor_usage(0), stereo_camera_usage(0), selected(false) {}
};
struct Capabilities {
    std::string status, availability, selected_camera_config_id;
    Error error;
    bool initialized, sdk_loaded;
    int depth_supported; // -1 unknown, 0 unsupported, 1 supported by runtime/device.
    uint64_t generation;
    std::vector<CameraConfig> cameras;
    Capabilities() : status("stopped"), availability("unknown"), initialized(false),
        sdk_loaded(false), depth_supported(-1), generation(0) {}
};
struct Hit {
    Pose pose;
    std::string plane_id;
    float distance;
    uint64_t frame_id, generation;
};
struct HitRequest {
    float x, y;
    uint64_t expected_frame, expected_generation;
    std::vector<Hit> hits;
    Error error;
    HitRequest() : x(0), y(0), expected_frame(0), expected_generation(0) {}
};
struct AnchorRequest {
    Pose pose;
    std::string plane_id, handle;
    uint64_t expected_frame, expected_generation;
    Error error;
    AnchorRequest() : expected_frame(0), expected_generation(0) {}
};
struct ReleaseRequest { std::string handle; Error error; };

// Slots are reused, tokens never are. No SDK pointer or numeric Lua precision is
// involved in a handle. Session generation changes on every accepted start.
template <typename T, size_t N> class HandleRegistry {
    struct Slot { std::string token; T value; Slot() : value() {} } slots[N];
    uint64_t generation, serial;
public:
    HandleRegistry() : generation(0), serial(0) {}
    void Reset(uint64_t g) { generation = g; serial = 0; for (size_t i = 0; i < N; ++i) { slots[i].token.clear(); slots[i].value = T(); } }
    size_t Size() const { size_t n = 0; for (size_t i = 0; i < N; ++i) n += !slots[i].token.empty(); return n; }
    std::string Insert(T value) {
        for (size_t i = 0; i < N; ++i) if (slots[i].token.empty()) {
            char token[80]; snprintf(token, sizeof(token), "a:%llu:%llu", (unsigned long long)generation, (unsigned long long)++serial);
            slots[i].value = value; slots[i].token = token; return token;
        }
        return std::string();
    }
    T* Find(const std::string& token) { for (size_t i = 0; i < N; ++i) if (!token.empty() && slots[i].token == token) return &slots[i].value; return 0; }
    bool Erase(const std::string& token) { for (size_t i = 0; i < N; ++i) if (!token.empty() && slots[i].token == token) { slots[i].token.clear(); slots[i].value = T(); return true; } return false; }
    template <typename F> void Each(F f) { for (size_t i = 0; i < N; ++i) if (!slots[i].token.empty()) f(slots[i].token, slots[i].value); }
};
inline bool ImageReady(uint64_t now, uint64_t last) { return last != 0 && now >= last && now - last < IMAGE_STALL_US; }
inline float TopLeftY(float y, int height) { return (float)height - y; }
}
#endif
