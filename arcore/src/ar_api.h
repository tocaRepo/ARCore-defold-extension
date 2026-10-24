#ifndef DEFOLD_ARCORE_API_H
#define DEFOLD_ARCORE_API_H
#include "arcore_c_api.h"

// Mandatory exports: publish atomically, verified against the packaged AAR.
#define AR_FUNCTIONS(X) \
    X(ArCoreApk_checkAvailability) X(ArSession_create) X(ArSession_destroy) \
    X(ArSession_resume) X(ArSession_pause) X(ArSession_configure) \
    X(ArSession_setDisplayGeometry) X(ArSession_update) X(ArSession_acquireNewAnchor) \
    X(ArConfig_create) X(ArConfig_destroy) X(ArConfig_setPlaneFindingMode) \
    X(ArConfig_setLightEstimationMode) X(ArConfig_setDepthMode) \
    X(ArConfig_setFocusMode) X(ArConfig_setUpdateMode) X(ArConfig_setTextureUpdateMode) \
    X(ArFrame_create) X(ArFrame_destroy) X(ArFrame_acquireCamera) \
    X(ArFrame_getTimestamp) X(ArFrame_acquireCameraImage) X(ArFrame_transformCoordinates2d) \
    X(ArCamera_release) X(ArCamera_getTrackingState) X(ArCamera_getViewMatrix) \
    X(ArCamera_getProjectionMatrix) X(ArCamera_getDisplayOrientedPose) \
    X(ArPose_create) X(ArPose_destroy) X(ArPose_getPoseRaw) \
    X(ArAnchor_getPose) X(ArAnchor_getTrackingState) X(ArAnchor_detach) X(ArAnchor_release) \
    X(ArImage_getWidth) X(ArImage_getHeight) X(ArImage_getFormat) \
    X(ArImage_getPlanePixelStride) X(ArImage_getPlaneRowStride) X(ArImage_getPlaneData) \
    X(ArImage_release) X(ArSession_getAllTrackables) \
    X(ArTrackableList_create) X(ArTrackableList_destroy) X(ArTrackableList_getSize) \
    X(ArTrackableList_acquireItem) X(ArTrackable_getTrackingState) X(ArTrackable_release) \
    X(ArTrackable_acquireNewAnchor) X(ArPlane_acquireSubsumedBy) X(ArPlane_getType) \
    X(ArPlane_getCenterPose) X(ArPlane_getExtentX) X(ArPlane_getExtentZ) X(ArPlane_isPoseInPolygon) \
    X(ArPlane_getPolygonSize) X(ArPlane_getPolygon) X(ArTrackable_getType) \
    X(ArFrame_hitTest) X(ArHitResultList_create) X(ArHitResultList_destroy) \
    X(ArHitResultList_getSize) X(ArHitResultList_getItem) X(ArHitResult_create) \
    X(ArHitResult_destroy) X(ArHitResult_getDistance) X(ArHitResult_getHitPose) \
    X(ArHitResult_acquireTrackable) X(ArCamera_getTrackingFailureReason) \
    X(ArCameraConfig_create) X(ArCameraConfig_destroy) X(ArCameraConfig_getCameraId) \
    X(ArCameraConfig_getImageDimensions) X(ArCameraConfig_getTextureDimensions) \
    X(ArCameraConfig_getFpsRange) X(ArCameraConfig_getFacingDirection) \
    X(ArCameraConfig_getDepthSensorUsage) X(ArCameraConfig_getStereoCameraUsage) \
    X(ArCameraConfigList_create) X(ArCameraConfigList_destroy) \
    X(ArCameraConfigList_getSize) X(ArCameraConfigList_getItem) \
    X(ArCameraConfigFilter_create) X(ArCameraConfigFilter_destroy) \
    X(ArCameraConfigFilter_setFacingDirection) X(ArCameraConfigFilter_setTargetFps) \
    X(ArSession_getSupportedCameraConfigsWithFilter) X(ArSession_getCameraConfig) \
    X(ArSession_setCameraConfig) X(ArSession_isDepthModeSupported) X(ArString_release)

namespace arcore {
struct Api {
#define AR_DECLARE(n) decltype(&::n) n;
    AR_FUNCTIONS(AR_DECLARE)
#undef AR_DECLARE
};
extern Api api;
const char* TrackingState(ArTrackingState state);
const char* FailureReason(ArTrackingFailureReason reason);
}
#endif
