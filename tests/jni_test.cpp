#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "fixture.h"
#include "ar_worker.h"
using namespace arcore;
using namespace fixture;

static Session session;
static FrameSnapshot frame;
static int width=720,height=1280;
static void Start(JNIEnv* env,void* p){ session.Start(env,p?*static_cast<Options*>(p):Options()); }
static void Stop(JNIEnv*,void*){session.Stop();}
static void Poll(JNIEnv* env,void*){session.Poll(env,width,height,frame);}
static void Active(JNIEnv*,void* p){session.SetActive(*static_cast<bool*>(p));}
static void Create(JNIEnv*,void* p){session.CreateAnchor(*static_cast<AnchorRequest*>(p));}
static void Drop(JNIEnv*,void* p){session.ReleaseAnchor(*static_cast<ReleaseRequest*>(p));}
static void HitTest(JNIEnv*,void* p){session.HitTest(*static_cast<HitRequest*>(p));}
static void CapabilitiesJob(JNIEnv*,void* p){session.GetCapabilities(*static_cast<arcore::Capabilities*>(p));}
static void Finalize(JNIEnv* env,void*){session.Finalize(env);}
static void EngineDetach(){JNIEnv* env=0;assert(vm->AttachCurrentThread((void**)&env,0)==JNI_OK);jclass c=env->FindClass("java/lang/Object");assert(c&&!env->ExceptionCheck());env->DeleteLocalRef(c);assert(vm->DetachCurrentThread()==JNI_OK);}
static void AllReleased(){assert(!images_live&&!cameras_live&&!poses_live&&!anchors_live&&!planes_refs&&!sessions_live&&!lists_live&&!strings_live);}

int main(int argc,char** argv){
    assert(argc==2);
    std::string classpath="-Djava.class.path="+std::string(argv[1]);
    JavaVMOption opts[2];opts[0].optionString=const_cast<char*>(classpath.c_str());opts[1].optionString=const_cast<char*>("-Xcheck:jni");
    JavaVMInitArgs args={};args.version=JNI_VERSION_1_8;args.nOptions=2;args.options=opts;
    JNIEnv* env=0;assert(JNI_CreateJavaVM(&vm,(void**)&env,&args)==JNI_OK);
    jclass cls=env->FindClass("android/app/Activity");assert(cls&&!env->ExceptionCheck());
    jmethodID ctor=env->GetMethodID(cls,"<init>","()V");assert(ctor&&!env->ExceptionCheck());
    jobject obj=env->NewObject(cls,ctor);assert(obj&&!env->ExceptionCheck());
    activity=env->NewGlobalRef(obj);env->DeleteLocalRef(obj);env->DeleteLocalRef(cls);
    ANativeActivity native={vm,activity};android_app app={&native};dmAndroid::SetAndroidApp(&app);
    vm->DetachCurrentThread();
    ArWorker worker;
    // A failed symbol lookup must not publish a partially populated API.
    missing_symbol=true;assert(worker.Invoke(Start,0));assert(worker.Invoke(Poll,0));assert(frame.status=="sdk_load_failed");
    missing_symbol=false;assert(worker.Invoke(Start,0));
    SetJavaInt("permission",0);assert(worker.Invoke(Poll,0));assert(frame.status=="permission_required"&&!frame.complete);
    SetJavaInt("permission",-1);assert(worker.Invoke(Poll,0));assert(frame.status=="permission_denied");
    SetJavaInt("permission",1);assert(worker.Invoke(Poll,0));assert(frame.fresh&&frame.tracking&&frame.complete);
    assert(images_live==0&&cameras_live==0&&frame.rgb.size()==27);
    assert(frame.planes.size()==1&&frame.planes[0].polygon.size()==8&&frame.planes[0].orientation=="horizontal_up");
    first_plane=frame.planes[0].id;
    arcore::Capabilities caps;assert(worker.Invoke(CapabilitiesJob,&caps));
    assert(caps.initialized&&caps.sdk_loaded&&caps.cameras.size()==2&&caps.depth_supported==1);
    assert(camera_selections==0&&caps.cameras[0].selected&&!caps.cameras[1].selected);
    std::string alternative=caps.cameras[1].id;
    puts("PASS loader atomicity, permission readiness, camera enumeration/default preservation");

    HitRequest hits;hits.x=125;hits.y=250;assert(worker.Invoke(HitTest,&hits));
    assert(hits.error.Ok()&&hits.hits.size()==2&&hits.hits[0].distance==1&&hits.hits[1].distance==2);
    assert(last_hit_x==125&&last_hit_y==1030);
    AnchorRequest a;a.pose=hits.hits[0].pose;a.plane_id=hits.hits[0].plane_id;a.expected_frame=hits.hits[0].frame_id;a.expected_generation=hits.hits[0].generation;
    assert(worker.Invoke(Create,&a));assert(a.error.Ok()&&!a.handle.empty());
    AnchorRequest b;b.pose.raw[4]=0.5f;assert(worker.Invoke(Create,&b));assert(b.error.Ok()&&b.handle!=a.handle);
    assert(anchors_live==2&&free_creations==1&&attached_creations==1);
    AnchorRequest invalid;invalid.plane_id="p:invalid";int old_free=free_creations;assert(worker.Invoke(Create,&invalid));assert(invalid.error.code=="invalid_plane"&&free_creations==old_free);
    force_anchor_fail=true;AnchorRequest failed;failed.plane_id=a.plane_id;assert(worker.Invoke(Create,&failed));assert(failed.error.code=="resource_exhausted"&&free_creations==old_free);force_anchor_fail=false;
    outside_polygon=true;HitRequest outside;outside.x=1;outside.y=1;assert(worker.Invoke(HitTest,&outside));assert(outside.hits.empty());
    AnchorRequest off;off.plane_id=a.plane_id;assert(worker.Invoke(Create,&off));assert(off.error.code=="outside_polygon");outside_polygon=false;
    assert(worker.Invoke(Poll,0));assert(frame.anchors.size()==2);
    ReleaseRequest drop;drop.handle=a.handle;assert(worker.Invoke(Drop,&drop));assert(drop.error.Ok()&&anchors_live==1);
    ReleaseRequest again;again.handle=a.handle;assert(worker.Invoke(Drop,&again));assert(again.error.code=="invalid_anchor");
    assert(worker.Invoke(Poll,0));assert(frame.anchors.size()==1&&frame.anchors[0].handle==b.handle);
    while(anchors_live<16){AnchorRequest r;assert(worker.Invoke(Create,&r));assert(r.error.Ok());}
    AnchorRequest full;assert(worker.Invoke(Create,&full));assert(full.error.code=="resource_exhausted"&&anchors_live==16);
    puts("PASS plane hits, pixel origin, attached/free anchors, no fallback, independent release, stale handles, exhaustion");

    for(int i=0;i<1010;++i){
        EngineDetach();now+=1000;assert(worker.Invoke(Poll,0));
        assert(frame.fresh&&frame.tracking&&frame.complete);
        assert(frame.view[0]==frame.projection[0]&&frame.view[0]==frame.camera_pose.raw[4]&&frame.uv[0]==frame.view[0]);
        int n=(int)frame.view[0];int expected=(298*(n%200)+128)>>8;assert(frame.rgb[0]==(expected>255?255:expected));
        assert(frame.anchors.size()==16&&images_live==0&&cameras_live==0);
    }
    // Already-attached engine callers retain their own attachment after a job.
    assert(vm->AttachCurrentThread((void**)&env,0)==JNI_OK);assert(worker.Invoke(Poll,0));JNIEnv* after=0;assert(vm->GetEnv((void**)&after,JNI_VERSION_1_8)==JNI_OK&&after==env);vm->DetachCurrentThread();
    uint64_t retained_id=frame.frame_id;float retained_view=frame.view[0];std::vector<uint8_t> retained_pixels=frame.rgb;
    duplicate_timestamp=true;assert(worker.Invoke(Poll,0));assert(!frame.fresh&&frame.frame_id==retained_id&&frame.view[0]==retained_view&&frame.rgb==retained_pixels);
    HitRequest stale_sdk;assert(worker.Invoke(HitTest,&stale_sdk));assert(stale_sdk.error.code=="image_not_current");duplicate_timestamp=false;
    image_available=false;now+=249999;assert(worker.Invoke(Poll,0));assert(!frame.fresh&&frame.tracking&&frame.frame_id==retained_id);
    ++now;assert(worker.Invoke(Poll,0));assert(!frame.tracking&&frame.status=="camera_image_unavailable"&&frame.view[0]==retained_view);
    image_available=true;assert(worker.Invoke(Poll,0));assert(frame.fresh&&frame.tracking);
    HitRequest stale;stale.expected_frame=retained_id;assert(worker.Invoke(HitTest,&stale));assert(stale.error.code=="stale_frame");
    puts("PASS 1,010 real-JVM updates amid engine detach, same-frame snapshots, image reuse/stall and stale SDK-frame gating");

    for(int rotation=0;rotation<4;++rotation){
        SetJavaInt("displayRotation",rotation);width=rotation%2?1280:720;height=rotation%2?720:1280;
        duplicate_timestamp=true;assert(worker.Invoke(Poll,0));
        if(rotation)assert(!frame.tracking&&frame.status=="display_geometry_changed");
        duplicate_timestamp=false;assert(worker.Invoke(Poll,0));assert(frame.fresh&&frame.tracking&&frame.display_rotation==rotation);
        HitRequest r;r.x=100;r.y=20;assert(worker.Invoke(HitTest,&r));assert(r.error.Ok()&&last_hit_y==height-20&&last_rotation==rotation&&last_width==width&&last_height==height);
    }
    plane_merge=true;plane_stopped=true;assert(worker.Invoke(Poll,0));
    bool merge_seen=false;for(auto& p:frame.planes)if(p.id==first_plane){assert(p.state=="stopped"&&!p.parent_id.empty());merge_seen=true;}assert(merge_seen);
    AnchorRequest merged;merged.plane_id=first_plane;assert(worker.Invoke(Create,&merged));assert(merged.error.code=="resource_exhausted"); // Registry capacity is checked first.
    ReleaseRequest spare;spare.handle=b.handle;assert(worker.Invoke(Drop,&spare));
    AnchorRequest stopped;stopped.plane_id=first_plane;assert(worker.Invoke(Create,&stopped));assert(stopped.error.code=="plane_not_tracking");
    alternate_plane=true;assert(worker.Invoke(Poll,0));assert(frame.planes.size()==3);
    bool down=false;for(auto& p:frame.planes)if(p.orientation=="horizontal_down")down=true;assert(down);
    plane_paused=true;assert(worker.Invoke(Poll,0));assert(!frame.tracking&&frame.tracking_failure_reason=="insufficient_light");plane_paused=false;
    puts("PASS rotation/resize, plane subsumption, stopped tombstones, polygon geometry and tracking diagnostics");

    for(int i=0;i<10;++i){bool active=false;assert(worker.Invoke(Active,&active));assert(worker.Invoke(Poll,0));assert(frame.status=="paused"&&!frame.tracking);active=true;assert(worker.Invoke(Active,&active));assert(worker.Invoke(Poll,0));assert(frame.fresh&&frame.tracking);}
    SetJavaInt("permission",-1);assert(worker.Invoke(Poll,0));assert(frame.status=="permission_denied"&&!frame.tracking);
    SetJavaInt("permission",1);assert(worker.Invoke(Poll,0));assert(frame.tracking);
    update_fail=true;assert(worker.Invoke(Poll,0));assert(frame.status=="frame_unavailable"&&!frame.tracking);update_fail=false;
    assert(worker.Invoke(Stop,0));AllReleased();assert(worker.Invoke(Stop,0));AllReleased();
    plane_merge=plane_stopped=alternate_plane=false;
    Options selected;selected.camera_config_id=alternative;assert(worker.Invoke(Start,&selected));assert(worker.Invoke(Poll,0));assert(frame.tracking&&camera_selections==1);
    assert(frame.planes[0].id!=first_plane);ReleaseRequest previous;previous.handle=b.handle;assert(worker.Invoke(Drop,&previous));assert(previous.error.code=="invalid_anchor");
    assert(worker.Invoke(Stop,0));AllReleased();
    Options bad;bad.camera_config_id="not-exposed";assert(worker.Invoke(Start,&bad));assert(worker.Invoke(Poll,0));assert(frame.status=="invalid_camera_config"&&!frame.tracking);
    assert(worker.Invoke(Start,0));assert(worker.Invoke(Poll,0));assert(frame.tracking);
    worker.Close(Finalize);AllReleased();
    assert(vm->AttachCurrentThread((void**)&env,0)==JNI_OK);VerifyJoined(env);ResetIdentity(env);env->DeleteGlobalRef(activity);
    puts("PASS pause/resume, revocation, update failure, stop/restart, camera selection, zero leaked SDK references, cleanup before JVM detach and final join");
    printf("PASS real-JVM worker regression (%d instrumented SDK/JNI calls)\n",worker_calls);
    assert(vm->DestroyJavaVM()==JNI_OK);return 0;
}
