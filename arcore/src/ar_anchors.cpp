#if defined(DM_PLATFORM_ANDROID) || defined(ARCORE_HOST_TEST)
#include "ar_session.h"

namespace arcore {
void Session::SnapshotAnchors(std::vector<AnchorSnapshot>& out) {
    out.clear();
    anchors.Each([this, &out](const std::string& handle, AnchorEntry& entry) {
        AnchorSnapshot a; a.handle = handle; a.plane_id = entry.plane_id;
        ArTrackingState state; api.ArAnchor_getTrackingState(session, entry.anchor, &state); a.state = TrackingState(state);
        api.ArAnchor_getPose(session, entry.anchor, pose); api.ArPose_getPoseRaw(session, pose, a.pose.raw);
        out.push_back(a);
    });
}
void Session::CreateAnchor(AnchorRequest& request) {
    if (!Interactive(request.error, request.expected_frame, request.expected_generation)) return;
    if (anchors.Size() >= MAX_ANCHORS) { request.error.Set("resource_exhausted", "The extension supports at most 16 live anchors."); return; }
    PlaneEntry* plane = request.plane_id.empty() ? 0 : FindPlane(request.plane_id);
    if (!request.plane_id.empty() && !plane) { request.error.Set("invalid_plane", "Unknown plane ID for this session."); return; }
    if (plane && !UsablePlane(plane->trackable)) { request.error.Set("plane_not_tracking", "Plane is paused, stopped or subsumed."); return; }
    ArPose* placement = 0; api.ArPose_create(session, request.pose.raw, &placement);
    if (plane) {
        int32_t inside = 0; api.ArPlane_isPoseInPolygon(session, reinterpret_cast<ArPlane*>(plane->trackable), placement, &inside);
        if (!inside) { api.ArPose_destroy(placement); request.error.Set("outside_polygon", "Attached placement is outside the observed plane polygon."); return; }
    }
    AnchorEntry entry; entry.plane_id = request.plane_id;
    ArStatus result = plane ? api.ArTrackable_acquireNewAnchor(session, plane->trackable, placement, &entry.anchor) :
        api.ArSession_acquireNewAnchor(session, placement, &entry.anchor);
    api.ArPose_destroy(placement);
    if (result != AR_SUCCESS || !entry.anchor) {
        if (entry.anchor) { api.ArAnchor_detach(session, entry.anchor); api.ArAnchor_release(entry.anchor); }
        request.error.Set(result == AR_ERROR_RESOURCE_EXHAUSTED ? "resource_exhausted" : "anchor_failed", "ARCore could not create the requested anchor.", result); return;
    }
    request.handle = anchors.Insert(entry);
}
void Session::ReleaseAnchor(ReleaseRequest& request) {
    AnchorEntry* entry = anchors.Find(request.handle);
    if (!entry) { request.error.Set("invalid_anchor", "Anchor handle is stale, released or unknown."); return; }
    api.ArAnchor_detach(session, entry->anchor); api.ArAnchor_release(entry->anchor); anchors.Erase(request.handle);
}
}
#endif
