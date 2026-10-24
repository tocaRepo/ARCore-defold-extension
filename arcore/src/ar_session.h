#ifndef DEFOLD_ARCORE_SESSION_H
#define DEFOLD_ARCORE_SESSION_H
#include <jni.h>
#include "ar_api.h"
#include "ar_types.h"

namespace arcore {
struct PlaneEntry { ArTrackable* trackable; std::string id, parent_id; };
struct AnchorEntry { ArAnchor* anchor; std::string plane_id; AnchorEntry() : anchor(0) {} };

// All methods and every owned SDK/JNI reference belong to ArWorker. The Lua
// bridge copies complete native results only after Invoke has returned.
class Session {
public:
    Session();
    void Start(JNIEnv* env, const Options& options);
    void Stop();
    void Finalize(JNIEnv* env);
    void SetActive(bool active);
    void Poll(JNIEnv* env, int width, int height, FrameSnapshot& out);
    void GetCapabilities(Capabilities& out) const;
    void HitTest(HitRequest& request);
    void CreateAnchor(AnchorRequest& request);
    void ReleaseAnchor(ReleaseRequest& request);
private:
    bool CheckJNI(JNIEnv* env);
    bool LoadApi(JNIEnv* env);
    bool LoadFailed(const char* reason);
    bool EnsureSession(JNIEnv* env, int width, int height);
    bool EnumerateCameras();
    CameraConfig ReadCameraConfig(ArCameraConfig* config);
    bool CopyImage(ArImage* image, FrameSnapshot& out);
    void RefreshPlanes(std::vector<PlaneSnapshot>& out);
    void SnapshotAnchors(std::vector<AnchorSnapshot>& out);
    std::string RegisterPlane(ArTrackable* acquired);
    PlaneEntry* FindPlane(const std::string& id);
    bool UsablePlane(ArTrackable* trackable);
    bool Interactive(Error& error, uint64_t expected_frame, uint64_t expected_generation);
    void Fail(const char* code, const char* message, int native_status = 0, bool terminal = false);

    ArSession* session;
    ArFrame* frame;
    ArPose* pose;
    ArTrackableList* surfaces;
    std::vector<PlaneEntry> planes;
    HandleRegistry<AnchorEntry, MAX_ANCHORS> anchors;
    jclass permission_class;
    jmethodID ensure_permission, get_rotation, reset_permission;
    bool active, requested, resumed, fatal, tracking;
    bool geometry_matches;
    uint64_t generation, update_id, next_plane_id, last_image_time;
    int64_t last_timestamp;
    int viewport_width, viewport_height, display_rotation;
    std::string status, detail, tracking_failure_reason;
    Error error;
    Options options;
    Capabilities capabilities;
    FrameSnapshot snapshot;
};
}
#endif
