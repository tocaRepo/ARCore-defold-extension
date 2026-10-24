#pragma once
#include <jni.h>
#include <stdint.h>
#include <string>
#include "ar_session.h"
namespace fixture {
extern JavaVM* vm;
extern jobject activity;
extern uint64_t now;
extern bool image_available, duplicate_timestamp, update_fail, force_anchor_fail;
extern bool plane_paused, plane_stopped, plane_merge, outside_polygon, alternate_plane;
extern bool missing_symbol;
extern float last_hit_x, last_hit_y;
extern int last_rotation, last_width, last_height;
extern int images_live, cameras_live, poses_live, anchors_live, planes_refs, sessions_live, lists_live, strings_live;
extern int free_creations, attached_creations, camera_selections, worker_calls;
extern std::string first_plane;
void CheckWorker();
void SetJavaInt(const char* field, int value);
void VerifyJoined(JNIEnv* env);
void ResetIdentity(JNIEnv* env);
}
