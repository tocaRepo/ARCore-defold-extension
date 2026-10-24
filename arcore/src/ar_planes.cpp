#if defined(DM_PLATFORM_ANDROID) || defined(ARCORE_HOST_TEST)
#include "ar_session.h"

namespace arcore {
std::string Session::RegisterPlane(ArTrackable* acquired) {
    if (!acquired) return std::string();
    for (size_t i = 0; i < planes.size(); ++i) if (planes[i].trackable == acquired) {
        api.ArTrackable_release(acquired); return planes[i].id;
    }
    char id[80]; snprintf(id, sizeof(id), "p:%llu:%llu", (unsigned long long)generation, (unsigned long long)++next_plane_id);
    PlaneEntry entry; entry.trackable = acquired; entry.id = id; planes.push_back(entry); return entry.id;
}
PlaneEntry* Session::FindPlane(const std::string& id) {
    for (size_t i = 0; i < planes.size(); ++i) if (planes[i].id == id) return &planes[i];
    return 0;
}
bool Session::UsablePlane(ArTrackable* trackable) {
    ArTrackingState state; api.ArTrackable_getTrackingState(session, trackable, &state);
    if (state != AR_TRACKING_STATE_TRACKING) return false;
    ArPlane* parent = 0; api.ArPlane_acquireSubsumedBy(session, reinterpret_cast<ArPlane*>(trackable), &parent);
    if (parent) { api.ArTrackable_release(reinterpret_cast<ArTrackable*>(parent)); return false; }
    return true;
}
void Session::RefreshPlanes(std::vector<PlaneSnapshot>& out) {
    api.ArSession_getAllTrackables(session, AR_TRACKABLE_PLANE, surfaces);
    int32_t count = 0; api.ArTrackableList_getSize(session, surfaces, &count);
    for (int i = 0; i < count; ++i) {
        ArTrackable* trackable = 0; api.ArTrackableList_acquireItem(session, surfaces, i, &trackable); RegisterPlane(trackable);
    }
    // Register parents before taking addresses/references into this vector.
    // RegisterPlane can reallocate it when a merge exposes a previously unseen parent.
    for (size_t i = 0; i < planes.size(); ++i) {
        ArPlane* parent = 0; api.ArPlane_acquireSubsumedBy(session, reinterpret_cast<ArPlane*>(planes[i].trackable), &parent);
        std::string parent_id = parent ? RegisterPlane(reinterpret_cast<ArTrackable*>(parent)) : std::string();
        planes[i].parent_id = parent_id;
    }
    out.clear(); out.reserve(planes.size());
    for (size_t i = 0; i < planes.size(); ++i) {
        PlaneSnapshot p; p.id = planes[i].id; p.parent_id = planes[i].parent_id;
        ArPlane* plane = reinterpret_cast<ArPlane*>(planes[i].trackable);
        ArTrackingState state; api.ArTrackable_getTrackingState(session, planes[i].trackable, &state); p.state = TrackingState(state);
        ArPlaneType type; api.ArPlane_getType(session, plane, &type);
        p.orientation = type == AR_PLANE_HORIZONTAL_UPWARD_FACING ? "horizontal_up" :
            type == AR_PLANE_HORIZONTAL_DOWNWARD_FACING ? "horizontal_down" : "vertical";
        api.ArPlane_getCenterPose(session, plane, pose); api.ArPose_getPoseRaw(session, pose, p.pose.raw);
        api.ArPlane_getExtentX(session, plane, &p.extent_x); api.ArPlane_getExtentZ(session, plane, &p.extent_z);
        int32_t size = 0; api.ArPlane_getPolygonSize(session, plane, &size);
        if (size >= 6 && size % 2 == 0) { p.polygon.resize((size_t)size); api.ArPlane_getPolygon(session, plane, p.polygon.data()); }
        out.push_back(p);
    }
}
}
#endif
