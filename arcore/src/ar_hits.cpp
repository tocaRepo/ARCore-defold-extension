#if defined(DM_PLATFORM_ANDROID) || defined(ARCORE_HOST_TEST)
#include <algorithm>
#include "ar_session.h"

namespace arcore {
void Session::HitTest(HitRequest& request) {
    if (!Interactive(request.error, request.expected_frame, request.expected_generation)) return;
    if (request.x < 0 || request.y < 0 || request.x > viewport_width || request.y > viewport_height) {
        request.error.Set("invalid_coordinates", "Hit coordinates must be physical pixels inside the last poll viewport."); return;
    }
    ArHitResultList* list = 0; ArHitResult* result = 0;
    api.ArHitResultList_create(session, &list); api.ArHitResult_create(session, &result);
    // ARCore view coordinates already honor display geometry. UV transforms are
    // for image sampling only; do not apply them again to view-space input.
    api.ArFrame_hitTest(session, frame, request.x, TopLeftY(request.y, viewport_height), list);
    int32_t count = 0; api.ArHitResultList_getSize(session, list, &count);
    for (int i = 0; i < count; ++i) {
        api.ArHitResultList_getItem(session, list, i, result);
        ArTrackable* trackable = 0; api.ArHitResult_acquireTrackable(session, result, &trackable);
        if (!trackable) continue;
        ArTrackableType type; api.ArTrackable_getType(session, trackable, &type);
        if (type != AR_TRACKABLE_PLANE || !UsablePlane(trackable)) { api.ArTrackable_release(trackable); continue; }
        api.ArHitResult_getHitPose(session, result, pose);
        int32_t inside = 0; api.ArPlane_isPoseInPolygon(session, reinterpret_cast<ArPlane*>(trackable), pose, &inside);
        if (!inside) { api.ArTrackable_release(trackable); continue; }
        Hit hit; api.ArPose_getPoseRaw(session, pose, hit.pose.raw); api.ArHitResult_getDistance(session, result, &hit.distance);
        hit.plane_id = RegisterPlane(trackable); // Consumes this acquired reference.
        hit.frame_id = snapshot.frame_id; hit.generation = generation; request.hits.push_back(hit);
    }
    api.ArHitResult_destroy(result); api.ArHitResultList_destroy(list);
    std::stable_sort(request.hits.begin(), request.hits.end(), [](const Hit& a, const Hit& b) { return a.distance < b.distance; });
}
}
#endif
