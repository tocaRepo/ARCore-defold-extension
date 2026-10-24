// Instrumented SDK stand-in. Production Session/worker code is compiled unchanged.
// Every export makes a real Java call and checks one continuous worker identity.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <vector>
#include <algorithm>
#include "fixture.h"
#include <dmsdk/dlib/android.h>
#include <dlfcn.h>
using namespace arcore;

struct ArSession_ { int selected; bool resumed; };
struct ArFrame_ { int number; int64_t timestamp; };
struct ArPose_ { float raw[7]; };
struct ArAnchor_ { arcore::Pose pose; bool detached; };
struct ArCamera_ { int number; };
struct ArImage_ { uint8_t bytes[3][16]; };
struct ArConfig_ {};
struct ArTrackable_ { int id, refs; };
struct ArTrackableList_ { std::vector<ArTrackable*> entries; };
struct ArHitResult_ { ArTrackable* trackable; int index; };
struct ArHitResultList_ { std::vector<ArTrackable*> entries; };
struct ArCameraConfig_ { int index; };
struct ArCameraConfigList_ {};
struct ArCameraConfigFilter_ {};

namespace fixture {
JavaVM* vm=0; jobject activity=0; uint64_t now=1000000;
bool image_available=true, duplicate_timestamp=false, update_fail=false, force_anchor_fail=false;
bool plane_paused=false, plane_stopped=false, plane_merge=false, outside_polygon=false, alternate_plane=false, missing_symbol=false;
float last_hit_x=0,last_hit_y=0;
int last_rotation=0,last_width=0,last_height=0;
int images_live=0,cameras_live=0,poses_live=0,anchors_live=0,planes_refs=0,sessions_live=0,lists_live=0,strings_live=0;
int free_creations=0,attached_creations=0,camera_selections=0,worker_calls=0;
std::string first_plane;
static std::thread::id worker_thread;
static JNIEnv* worker_env=0;
static jobject java_thread=0;
static jclass thread_class=0;
static jmethodID current_thread=0, is_alive=0;
void CheckWorker() {
    JNIEnv* env=0; assert(vm->GetEnv((void**)&env,JNI_VERSION_1_8)==JNI_OK && env);
    if (!worker_env) {
        worker_env=env; worker_thread=std::this_thread::get_id();
        jclass local=env->FindClass("java/lang/Thread"); assert(local && !env->ExceptionCheck());
        thread_class=(jclass)env->NewGlobalRef(local); env->DeleteLocalRef(local);
        current_thread=env->GetStaticMethodID(thread_class,"currentThread","()Ljava/lang/Thread;"); assert(current_thread && !env->ExceptionCheck());
        is_alive=env->GetMethodID(thread_class,"isAlive","()Z"); assert(is_alive && !env->ExceptionCheck());
        jobject t=env->CallStaticObjectMethod(thread_class,current_thread); assert(t && !env->ExceptionCheck());
        java_thread=env->NewGlobalRef(t); env->DeleteLocalRef(t);
    }
    assert(env==worker_env && std::this_thread::get_id()==worker_thread);
    jobject t=env->CallStaticObjectMethod(thread_class,current_thread); assert(t && !env->ExceptionCheck());
    assert(env->IsSameObject(t,java_thread)); env->DeleteLocalRef(t); ++worker_calls;
}
void VerifyJoined(JNIEnv* env) { assert(java_thread); jboolean alive=env->CallBooleanMethod(java_thread,is_alive); assert(!env->ExceptionCheck() && !alive); }
void ResetIdentity(JNIEnv* env) { env->DeleteGlobalRef(java_thread); env->DeleteGlobalRef(thread_class); java_thread=0; thread_class=0; worker_env=0; }
void SetJavaInt(const char* field,int value) {
    JNIEnv* env=0; assert(vm->AttachCurrentThread((void**)&env,0)==JNI_OK);
    jclass c=env->FindClass("com/defold/arcore/CameraPermission"); assert(c && !env->ExceptionCheck());
    jfieldID f=env->GetStaticFieldID(c,field,"I"); assert(f && !env->ExceptionCheck());
    env->SetStaticIntField(c,f,value); env->DeleteLocalRef(c); assert(!env->ExceptionCheck());
    vm->DetachCurrentThread();
}
static ArTrackable_ plane0={0,0},plane1={1,0},plane2={2,0},point={9,0};
static void Acquire(ArTrackable* t) { if(t){++t->refs;++planes_refs;} }
static void Release(ArTrackable* t) { if(t){assert(t->refs>0);--t->refs;--planes_refs;} }
}
using namespace fixture;
namespace dmTime { uint64_t GetMonotonicTime() { return fixture::now; } }
namespace dmGraphics { jobject GetNativeAndroidActivity() { CheckWorker();return fixture::activity; } }
namespace dmAndroid {
jclass LoadClass(JNIEnv* env,const char* name) { CheckWorker();std::string path=name;std::replace(path.begin(),path.end(),'.','/');return env->FindClass(path.c_str()); }
}
#define C CheckWorker()

void ArCoreApk_checkAvailability(void*,void*,ArAvailability* v){C;*v=AR_AVAILABILITY_SUPPORTED_INSTALLED;}
ArStatus ArSession_create(void*,void*,ArSession** out){C;*out=new ArSession_();(*out)->selected=0;(*out)->resumed=false;++sessions_live;return AR_SUCCESS;}
void ArSession_destroy(ArSession* s){C;assert(!s->resumed);delete s;--sessions_live;}
ArStatus ArSession_resume(ArSession* s){C;s->resumed=true;return AR_SUCCESS;}
ArStatus ArSession_pause(ArSession* s){C;s->resumed=false;return AR_SUCCESS;}
ArStatus ArSession_configure(ArSession*,const ArConfig*){C;return AR_SUCCESS;}
void ArSession_setDisplayGeometry(ArSession*,int32_t r,int32_t w,int32_t h){C;last_rotation=r;last_width=w;last_height=h;}
ArStatus ArSession_update(ArSession* s,ArFrame* f){C;assert(s->resumed);if(update_fail)return AR_ERROR_CAMERA_NOT_AVAILABLE;++f->number;if(!duplicate_timestamp)f->timestamp=f->number*1000000;return AR_SUCCESS;}
ArStatus ArSession_acquireNewAnchor(ArSession*,const ArPose* p,ArAnchor** out){C;++free_creations;if(force_anchor_fail)return AR_ERROR_RESOURCE_EXHAUSTED;*out=new ArAnchor_();memcpy((*out)->pose.raw,p->raw,28);(*out)->detached=false;++anchors_live;return AR_SUCCESS;}
void ArConfig_create(const ArSession*,ArConfig** out){C;*out=new ArConfig_();++lists_live;}
void ArConfig_destroy(ArConfig* p){C;delete p;--lists_live;}
void ArConfig_setPlaneFindingMode(const ArSession*,ArConfig*,ArPlaneFindingMode m){C;assert(m==AR_PLANE_FINDING_MODE_HORIZONTAL_AND_VERTICAL);}
void ArConfig_setLightEstimationMode(const ArSession*,ArConfig*,ArLightEstimationMode m){C;assert(m==AR_LIGHT_ESTIMATION_MODE_DISABLED);}
void ArConfig_setDepthMode(const ArSession*,ArConfig*,ArDepthMode m){C;assert(m==AR_DEPTH_MODE_DISABLED);}
void ArConfig_setFocusMode(const ArSession*,ArConfig*,ArFocusMode m){C;assert(m==AR_FOCUS_MODE_AUTO);}
void ArConfig_setUpdateMode(const ArSession*,ArConfig*,ArUpdateMode m){C;assert(m==AR_UPDATE_MODE_LATEST_CAMERA_IMAGE);}
void ArConfig_setTextureUpdateMode(const ArSession*,ArConfig*,ArTextureUpdateMode m){C;assert(m==AR_TEXTURE_UPDATE_MODE_EXPOSE_HARDWARE_BUFFER);}
void ArFrame_create(const ArSession*,ArFrame** out){C;*out=new ArFrame_();(*out)->number=0;(*out)->timestamp=0;++lists_live;}
void ArFrame_destroy(ArFrame* f){C;delete f;--lists_live;}
void ArFrame_acquireCamera(const ArSession*,const ArFrame* f,ArCamera** out){C;*out=new ArCamera_();(*out)->number=f->number;++cameras_live;}
void ArFrame_getTimestamp(const ArSession*,const ArFrame* f,int64_t* out){C;*out=f->timestamp;}
ArStatus ArFrame_acquireCameraImage(ArSession*,ArFrame* f,ArImage** out){C;if(!image_available)return AR_ERROR_NOT_YET_AVAILABLE;*out=new ArImage_();memset((*out)->bytes[0],16+f->number%200,16);memset((*out)->bytes[1],128,16);memset((*out)->bytes[2],128,16);++images_live;return AR_SUCCESS;}
void ArFrame_transformCoordinates2d(const ArSession*,const ArFrame* f,ArCoordinates2dType in,int32_t n,const float* src,ArCoordinates2dType out,float* dst){C;assert(in==AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES && out==AR_COORDINATES_2D_IMAGE_NORMALIZED && n==3);assert(src[0]==-1 && src[1]==-1);for(int i=0;i<6;++i)dst[i]=(float)f->number+i;}
void ArCamera_release(ArCamera* p){C;delete p;--cameras_live;}
void ArCamera_getTrackingState(const ArSession*,const ArCamera*,ArTrackingState* out){C;*out=plane_paused?AR_TRACKING_STATE_PAUSED:AR_TRACKING_STATE_TRACKING;}
void ArCamera_getViewMatrix(const ArSession*,const ArCamera* p,float* out){C;for(int i=0;i<16;++i)out[i]=(float)p->number;}
void ArCamera_getProjectionMatrix(const ArSession*,const ArCamera* p,float near_clip,float far_clip,float* out){C;assert(near_clip==0.05f && far_clip==50.f);for(int i=0;i<16;++i)out[i]=(float)p->number;}
void ArCamera_getDisplayOrientedPose(const ArSession*,const ArCamera* p,ArPose* out){C;Pose identity;memcpy(out->raw,identity.raw,28);out->raw[4]=(float)p->number;}
void ArPose_create(const ArSession*,const float* p,ArPose** out){C;*out=new ArPose_();Pose identity;memcpy((*out)->raw,p?p:identity.raw,28);++poses_live;}
void ArPose_destroy(ArPose* p){C;delete p;--poses_live;}
void ArPose_getPoseRaw(const ArSession*,const ArPose* p,float* out){C;memcpy(out,p->raw,28);}
void ArAnchor_getPose(const ArSession*,const ArAnchor* p,ArPose* out){C;memcpy(out->raw,p->pose.raw,28);}
void ArAnchor_getTrackingState(const ArSession*,const ArAnchor* p,ArTrackingState* out){C;*out=p->detached?AR_TRACKING_STATE_STOPPED:AR_TRACKING_STATE_TRACKING;}
void ArAnchor_detach(ArSession*,ArAnchor* p){C;assert(!p->detached);p->detached=true;}
void ArAnchor_release(ArAnchor* p){C;assert(p->detached);delete p;--anchors_live;}
void ArImage_getWidth(const ArSession*,const ArImage*,int32_t* out){C;*out=3;}
void ArImage_getHeight(const ArSession*,const ArImage*,int32_t* out){C;*out=3;}
void ArImage_getFormat(const ArSession*,const ArImage*,ArImageFormat* out){C;*out=AR_IMAGE_FORMAT_YUV_420_888;}
void ArImage_getPlanePixelStride(const ArSession*,const ArImage*,int32_t i,int32_t* out){C;*out=i?2:1;}
void ArImage_getPlaneRowStride(const ArSession*,const ArImage*,int32_t,int32_t* out){C;*out=4;}
void ArImage_getPlaneData(const ArSession*,const ArImage* p,int32_t i,const uint8_t** out,int32_t* size){C;*out=p->bytes[i];*size=16;}
void ArImage_release(ArImage* p){C;delete p;--images_live;}
void ArSession_getAllTrackables(const ArSession*,ArTrackableType type,ArTrackableList* out){C;assert(type==AR_TRACKABLE_PLANE);for(auto t:out->entries)Release(t);out->entries.clear();out->entries.push_back(alternate_plane?&plane2:&plane0);for(auto t:out->entries)Acquire(t);}
void ArTrackableList_create(const ArSession*,ArTrackableList** out){C;*out=new ArTrackableList_();++lists_live;}
void ArTrackableList_destroy(ArTrackableList* p){C;for(auto t:p->entries)Release(t);delete p;--lists_live;}
void ArTrackableList_getSize(const ArSession*,const ArTrackableList* p,int32_t* out){C;*out=(int32_t)p->entries.size();}
void ArTrackableList_acquireItem(const ArSession*,const ArTrackableList* p,int32_t i,ArTrackable** out){C;*out=p->entries[i];Acquire(*out);}
void ArTrackable_getTrackingState(const ArSession*,const ArTrackable* p,ArTrackingState* out){C;*out=p->id==0&&plane_stopped?AR_TRACKING_STATE_STOPPED:plane_paused?AR_TRACKING_STATE_PAUSED:AR_TRACKING_STATE_TRACKING;}
void ArTrackable_release(ArTrackable* p){C;Release(p);}
ArStatus ArTrackable_acquireNewAnchor(ArSession*,ArTrackable*,ArPose* p,ArAnchor** out){C;++attached_creations;if(force_anchor_fail)return AR_ERROR_RESOURCE_EXHAUSTED;*out=new ArAnchor_();memcpy((*out)->pose.raw,p->raw,28);(*out)->detached=false;++anchors_live;return AR_SUCCESS;}
void ArPlane_acquireSubsumedBy(const ArSession*,const ArPlane* p,ArPlane** out){C;ArTrackable* t=(ArTrackable*)p;*out=plane_merge&&t->id==0?(ArPlane*)&plane1:0;if(*out)Acquire((ArTrackable*)*out);}
void ArPlane_getType(const ArSession*,const ArPlane* p,ArPlaneType* out){C;int id=((const ArTrackable*)p)->id;*out=id==0?AR_PLANE_HORIZONTAL_UPWARD_FACING:id==1?AR_PLANE_VERTICAL:AR_PLANE_HORIZONTAL_DOWNWARD_FACING;}
void ArPlane_getCenterPose(const ArSession*,const ArPlane*,ArPose* out){C;Pose identity;memcpy(out->raw,identity.raw,28);}
void ArPlane_getExtentX(const ArSession*,const ArPlane*,float* out){C;*out=2;}
void ArPlane_getExtentZ(const ArSession*,const ArPlane*,float* out){C;*out=2;}
void ArPlane_isPoseInPolygon(const ArSession*,const ArPlane*,const ArPose* p,int32_t* out){C;*out=!outside_polygon && p->raw[4]>=-1 && p->raw[4]<=1 && p->raw[6]>=-1 && p->raw[6]<=1;}
void ArPlane_getPolygonSize(const ArSession*,const ArPlane*,int32_t* out){C;*out=8;}
void ArPlane_getPolygon(const ArSession*,const ArPlane*,float* out){C;float polygon[]={-1,-1,1,-1,1,1,-1,1};memcpy(out,polygon,32);}
void ArTrackable_getType(const ArSession*,const ArTrackable* p,ArTrackableType* out){C;*out=p->id==9?AR_TRACKABLE_POINT:AR_TRACKABLE_PLANE;}
void ArFrame_hitTest(const ArSession*,const ArFrame*,float x,float y,ArHitResultList* out){C;last_hit_x=x;last_hit_y=y;for(auto t:out->entries)Release(t);out->entries={&plane1,&plane0,&point};for(auto t:out->entries)Acquire(t);}
void ArHitResultList_create(const ArSession*,ArHitResultList** out){C;*out=new ArHitResultList_();++lists_live;}
void ArHitResultList_destroy(ArHitResultList* p){C;for(auto t:p->entries)Release(t);delete p;--lists_live;}
void ArHitResultList_getSize(const ArSession*,const ArHitResultList* p,int32_t* out){C;*out=(int32_t)p->entries.size();}
void ArHitResultList_getItem(const ArSession*,const ArHitResultList* p,int32_t i,ArHitResult* out){C;Release(out->trackable);out->trackable=p->entries[i];Acquire(out->trackable);out->index=i;}
void ArHitResult_create(const ArSession*,ArHitResult** out){C;*out=new ArHitResult_();(*out)->trackable=0;++lists_live;}
void ArHitResult_destroy(ArHitResult* p){C;Release(p->trackable);delete p;--lists_live;}
void ArHitResult_getDistance(const ArSession*,const ArHitResult* p,float* out){C;*out=p->index==0?2.f:1.f;}
void ArHitResult_getHitPose(const ArSession*,const ArHitResult*,ArPose* out){C;Pose identity;memcpy(out->raw,identity.raw,28);}
void ArHitResult_acquireTrackable(const ArSession*,const ArHitResult* p,ArTrackable** out){C;*out=p->trackable;Acquire(*out);}
void ArCamera_getTrackingFailureReason(const ArSession*,const ArCamera*,ArTrackingFailureReason* out){C;*out=plane_paused?AR_TRACKING_FAILURE_REASON_INSUFFICIENT_LIGHT:AR_TRACKING_FAILURE_REASON_NONE;}
void ArCameraConfig_create(const ArSession*,ArCameraConfig** out){C;*out=new ArCameraConfig_();(*out)->index=0;++lists_live;}
void ArCameraConfig_destroy(ArCameraConfig* p){C;delete p;--lists_live;}
void ArCameraConfig_getCameraId(const ArSession*,const ArCameraConfig* p,char** out){C;*out=new char[2];(*out)[0]='0'+(char)p->index;(*out)[1]=0;++strings_live;}
void ArCameraConfig_getImageDimensions(const ArSession*,const ArCameraConfig* p,int32_t* w,int32_t* h){C;*w=p->index?1280:640;*h=p->index?720:480;}
void ArCameraConfig_getTextureDimensions(const ArSession*,const ArCameraConfig*,int32_t* w,int32_t* h){C;*w=1920;*h=1080;}
void ArCameraConfig_getFpsRange(const ArSession*,const ArCameraConfig*,int32_t* a,int32_t* b){C;*a=30;*b=30;}
void ArCameraConfig_getFacingDirection(const ArSession*,const ArCameraConfig*,ArCameraConfigFacingDirection* out){C;*out=AR_CAMERA_CONFIG_FACING_DIRECTION_BACK;}
void ArCameraConfig_getDepthSensorUsage(const ArSession*,const ArCameraConfig*,uint32_t* out){C;*out=AR_CAMERA_CONFIG_DEPTH_SENSOR_USAGE_DO_NOT_USE;}
void ArCameraConfig_getStereoCameraUsage(const ArSession*,const ArCameraConfig*,ArCameraConfigStereoCameraUsage* out){C;*out=AR_CAMERA_CONFIG_STEREO_CAMERA_USAGE_DO_NOT_USE;}
void ArCameraConfigList_create(const ArSession*,ArCameraConfigList** out){C;*out=new ArCameraConfigList_();++lists_live;}
void ArCameraConfigList_destroy(ArCameraConfigList* p){C;delete p;--lists_live;}
void ArCameraConfigList_getSize(const ArSession*,const ArCameraConfigList*,int32_t* out){C;*out=2;}
void ArCameraConfigList_getItem(const ArSession*,const ArCameraConfigList*,int32_t i,ArCameraConfig* out){C;out->index=i;}
void ArCameraConfigFilter_create(const ArSession*,ArCameraConfigFilter** out){C;*out=new ArCameraConfigFilter_();++lists_live;}
void ArCameraConfigFilter_destroy(ArCameraConfigFilter* p){C;delete p;--lists_live;}
void ArCameraConfigFilter_setFacingDirection(const ArSession*,ArCameraConfigFilter*,ArCameraConfigFacingDirection f){C;assert(f==AR_CAMERA_CONFIG_FACING_DIRECTION_BACK);}
void ArCameraConfigFilter_setTargetFps(const ArSession*,ArCameraConfigFilter*,uint32_t f){C;assert(f==(AR_CAMERA_CONFIG_TARGET_FPS_30|AR_CAMERA_CONFIG_TARGET_FPS_60));}
void ArSession_getSupportedCameraConfigsWithFilter(const ArSession*,const ArCameraConfigFilter*,ArCameraConfigList*){C;}
void ArSession_getCameraConfig(const ArSession* s,ArCameraConfig* out){C;out->index=s->selected;}
ArStatus ArSession_setCameraConfig(const ArSession* s,const ArCameraConfig* p){C;assert(!s->resumed);const_cast<ArSession*>(s)->selected=p->index;++camera_selections;return AR_SUCCESS;}
void ArSession_isDepthModeSupported(const ArSession*,ArDepthMode m,int32_t* out){C;assert(m==AR_DEPTH_MODE_AUTOMATIC);*out=1;}
void ArString_release(char* p){C;delete[] p;--strings_live;}

void* dlopen(const char* path,int){C;assert(std::string(path)=="fixture.apk!/lib/arm64-v8a/libarcore_sdk_c.so");return (void*)1;}
void* dlsym(void*,const char* name){C;if(missing_symbol && !strcmp(name,"ArPlane_getPolygon"))return 0;
#define FIND(n) if(!strcmp(name,#n)) return reinterpret_cast<void*>(&::n);
AR_FUNCTIONS(FIND)
#undef FIND
assert(false);return 0;}
int dlclose(void*){C;return 0;}
const char* dlerror(){C;return "fixture loader error";}
