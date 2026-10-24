#if defined(DM_PLATFORM_ANDROID) || defined(ARCORE_HOST_TEST)
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/log.h>
#include "ar_session.h"
#include "ar_image.h"

namespace arcore {
const char* TrackingState(ArTrackingState state) {
    return state == AR_TRACKING_STATE_TRACKING ? "tracking" : state == AR_TRACKING_STATE_STOPPED ? "stopped" : "paused";
}
const char* FailureReason(ArTrackingFailureReason reason) {
    switch (reason) {
    case AR_TRACKING_FAILURE_REASON_NONE: return "none";
    case AR_TRACKING_FAILURE_REASON_BAD_STATE: return "bad_state";
    case AR_TRACKING_FAILURE_REASON_INSUFFICIENT_LIGHT: return "insufficient_light";
    case AR_TRACKING_FAILURE_REASON_EXCESSIVE_MOTION: return "excessive_motion";
    case AR_TRACKING_FAILURE_REASON_INSUFFICIENT_FEATURES: return "insufficient_features";
    case AR_TRACKING_FAILURE_REASON_CAMERA_UNAVAILABLE: return "camera_unavailable";
    default: return "unknown";
    }
}
bool Session::CopyImage(ArImage* image, FrameSnapshot& out) {
    int32_t width = 0, height = 0; ArImageFormat format;
    api.ArImage_getWidth(session, image, &width); api.ArImage_getHeight(session, image, &height);
    api.ArImage_getFormat(session, image, &format);
    if (format != AR_IMAGE_FORMAT_YUV_420_888) return false;
    ImagePlane planes[3];
    for (int i = 0; i < 3; ++i) {
        int32_t length = 0, row = 0, stride = 0;
        api.ArImage_getPlaneData(session, image, i, &planes[i].data, &length);
        api.ArImage_getPlaneRowStride(session, image, i, &row); api.ArImage_getPlanePixelStride(session, image, i, &stride);
        planes[i].length = length; planes[i].row_stride = row; planes[i].pixel_stride = stride;
    }
    if (!ConvertYuv(width, height, planes, out.rgb)) return false;
    out.width = width; out.height = height; return true;
}
void Session::Poll(JNIEnv* env, int width, int height, FrameSnapshot& out) {
    snapshot.fresh = false;
    if (EnsureSession(env, width, height)) {
        ArStatus result = api.ArSession_update(session, frame);
        if (!CheckJNI(env)) { /* error is already recorded */ }
        else if (result != AR_SUCCESS) {
            Fail("frame_unavailable", "ARCore frame update failed.", result);
            // Do not allow an older SDK frame to become interactive again.
            geometry_matches = false;
        } else {
            ++update_id;
            ArCamera* camera = 0; api.ArFrame_acquireCamera(session, frame, &camera);
            ArTrackingState state; api.ArCamera_getTrackingState(session, camera, &state);
            ArTrackingFailureReason failure; api.ArCamera_getTrackingFailureReason(session, camera, &failure);
            tracking_failure_reason = FailureReason(failure);
            tracking = state == AR_TRACKING_STATE_TRACKING;
            status = tracking ? "tracking" : "finding_tracking";
            detail = tracking ? "" : tracking_failure_reason;
            int64_t timestamp = 0; api.ArFrame_getTimestamp(session, frame, &timestamp);
            if (timestamp != 0 && timestamp != last_timestamp) {
                ArImage* image = 0; result = api.ArFrame_acquireCameraImage(session, frame, &image);
                bool fresh = result == AR_SUCCESS && image && CopyImage(image, snapshot);
                if (image) api.ArImage_release(image); // Never hold a CPU image between jobs.
                if (fresh) {
                    last_timestamp = timestamp; last_image_time = dmTime::GetMonotonicTime();
                    api.ArCamera_getViewMatrix(session, camera, snapshot.view);
                    api.ArCamera_getProjectionMatrix(session, camera, options.near_clip, options.far_clip, snapshot.projection);
                    api.ArCamera_getDisplayOrientedPose(session, camera, pose); api.ArPose_getPoseRaw(session, pose, snapshot.camera_pose.raw);
                    float ndc[] = {-1, -1, 1, -1, -1, 1};
                    api.ArFrame_transformCoordinates2d(session, frame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES,
                        3, ndc, AR_COORDINATES_2D_IMAGE_NORMALIZED, snapshot.uv);
                    RefreshPlanes(snapshot.planes); SnapshotAnchors(snapshot.anchors);
                    snapshot.complete = snapshot.fresh = true; snapshot.timestamp = timestamp; snapshot.frame_id = update_id;
                    snapshot.viewport_width = width; snapshot.viewport_height = height; snapshot.display_rotation = display_rotation;
                    geometry_matches = true;
                }
            }
            api.ArCamera_release(camera);
            if (!ImageReady(dmTime::GetMonotonicTime(), last_image_time)) {
                Fail("camera_image_unavailable", "Waiting for a complete camera image (250 ms stall guard).", result);
            } else if (!geometry_matches) {
                Fail("display_geometry_changed", "Waiting for a camera image matching the new viewport or display rotation.");
            }
        }
    } else tracking = false;
    snapshot.status = active ? status : "paused"; snapshot.detail = detail; snapshot.error = error;
    snapshot.tracking = tracking; snapshot.tracking_failure_reason = tracking_failure_reason;
    snapshot.session_generation = generation;
    // Only complete image/transform/geometry/anchor bundles cross the boundary.
    // Live status is independent; a non-fresh poll never refreshes pose fields.
    out = snapshot;
}
bool Session::Interactive(Error& out, uint64_t expected_frame, uint64_t expected_generation) {
    if (!requested || !session || fatal) { out.Set("not_ready", "Start and poll a ready ARCore session first."); return false; }
    if (expected_generation && expected_generation != generation) { out.Set("stale_session", "Request belongs to an earlier session."); return false; }
    if (!active || !resumed || !tracking) { out.Set("not_tracking", "The camera is not tracking."); return false; }
    if (!snapshot.complete || !geometry_matches || !ImageReady(dmTime::GetMonotonicTime(), last_image_time) || snapshot.frame_id != update_id) {
        out.Set("image_not_current", "Poll a fresh image before interacting with this SDK frame."); return false;
    }
    if (expected_frame && expected_frame != snapshot.frame_id) { out.Set("stale_frame", "Request pose/hit belongs to an older frame."); return false; }
    return true;
}
}
#endif
