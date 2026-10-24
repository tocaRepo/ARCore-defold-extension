#include <assert.h>
#include <stdio.h>
#include <string>
#include "fixture.h"
// Include the production binding so its extension entry points are available.
#include "../arcore/src/arcore.cpp"

static void Lua(lua_State* L,const char* code){if(luaL_dostring(L,code)){fprintf(stderr,"LUA TEST FAILED: %s\n",lua_tostring(L,-1));abort();}assert(lua_gettop(L)==0);}
int main(int argc,char** argv){
    assert(argc==2);bridge::engine_thread=std::this_thread::get_id();
    std::string classpath="-Djava.class.path="+std::string(argv[1]);JavaVMOption opts[2];opts[0].optionString=const_cast<char*>(classpath.c_str());opts[1].optionString=const_cast<char*>("-Xcheck:jni");
    JavaVMInitArgs args={};args.version=JNI_VERSION_1_8;args.nOptions=2;args.options=opts;JNIEnv* env=0;
    assert(JNI_CreateJavaVM(&fixture::vm,(void**)&env,&args)==JNI_OK);
    jclass c=env->FindClass("android/app/Activity");assert(c&&!env->ExceptionCheck());jmethodID ctor=env->GetMethodID(c,"<init>","()V");assert(ctor&&!env->ExceptionCheck());
    jobject obj=env->NewObject(c,ctor);assert(obj&&!env->ExceptionCheck());fixture::activity=env->NewGlobalRef(obj);env->DeleteLocalRef(obj);env->DeleteLocalRef(c);
    ANativeActivity native={fixture::vm,fixture::activity};android_app app={&native};dmAndroid::SetAndroidApp(&app);fixture::vm->DetachCurrentThread();
    lua_State* L=luaL_newstate();luaL_openlibs(L);bridge::Register(L);dmExtension::Params params={L};Initialize(&params);
    Lua(L,"local ok,e=arcore.start({depth=true});assert(not ok and e.code=='unsupported_option')\nlocal ok,e=arcore.start({near_clip=0});assert(not ok and e.code=='invalid_options')\nlocal f,e=arcore.poll(0,1280);assert(not f and e.code=='invalid_viewport')\nassert(arcore.start())");
    bridge::fail_buffer=true;
    Lua(L,"local f,e=arcore.poll(720,1280);assert(not f and e.code=='buffer_unavailable')");
    fixture::duplicate_timestamp=true;
    Lua(L,"frame=assert(arcore.poll(720,1280));assert(frame.fresh and frame.complete and frame.pixels and frame.view and frame.projection)\nassert(frame.width==3 and frame.height==3 and #buffer.get_bytes(frame.pixels)==27)\nassert(#frame.planes==1 and frame.planes[1].orientation=='horizontal_up')");
    fixture::duplicate_timestamp=false;
    Lua(L,"frame=assert(arcore.poll(720,1280));\nlocal h=assert(arcore.hit_test(100,250,{frame_id=frame.frame_id,session_generation=frame.session_generation}));assert(#h==2 and h[1].distance<h[2].distance)\nfirst=assert(arcore.create_anchor({pose=h[1].pose,plane_id=h[1].plane_id,frame_id=h[1].frame_id,session_generation=h[1].session_generation}))\nsecond=assert(arcore.create_anchor({pose={position=vmath.vector3(0.5,0,0)}}));assert(first~=second)\nframe=assert(arcore.poll(720,1280));assert(#frame.anchors==2 and frame.anchors[1].frame_id==frame.frame_id)\nlocal ok,e=arcore.create_anchor({pose={position=vmath.vector3(),rotation=vmath.quat(0,0,0,0)}});assert(not ok and e.code=='invalid_pose')\nassert(arcore.release_anchor(first));local ok,e=arcore.release_anchor(first);assert(not ok and e.code=='invalid_anchor')\nlocal c=arcore.get_capabilities();assert(c.native and c.initialized and c.max_anchors==16 and #c.camera_configs==2 and c.runtime.depth=='supported' and not c.implementation.depth)\nsaved_id=frame.frame_id;saved_pixels=buffer.get_bytes(frame.pixels)");
    fixture::image_available=false;
    Lua(L,"frame=assert(arcore.poll(720,1280));assert(not frame.fresh and frame.frame_id==saved_id and buffer.get_bytes(frame.pixels)==saved_pixels)");
    fixture::now+=250000;
    Lua(L,"frame=assert(arcore.poll(720,1280));assert(not frame.tracking and frame.status=='camera_image_unavailable')\nassert(arcore.stop());assert(arcore.stop());local f=arcore.poll(720,1280);assert(not f.complete and not f.pixels and #f.planes==0)\nframe=nil;collectgarbage('collect')");
    assert(bridge::buffers_live==0);
    fixture::image_available=true;
    Lua(L,"assert(arcore.start());frame=arcore.poll(720,1280);assert(frame.tracking);local ok,e=arcore.release_anchor(second);assert(not ok and e.code=='invalid_anchor')");
    Finalize(&params);Lua(L,"frame=nil;collectgarbage('collect')");lua_close(L);assert(bridge::buffers_live==0);
    assert(!fixture::images_live&&!fixture::cameras_live&&!fixture::poses_live&&!fixture::anchors_live&&!fixture::planes_refs&&!fixture::sessions_live&&!fixture::lists_live);
    assert(fixture::vm->AttachCurrentThread((void**)&env,0)==JNI_OK);fixture::VerifyJoined(env);fixture::ResetIdentity(env);env->DeleteGlobalRef(fixture::activity);
    assert(fixture::vm->DestroyJavaVM()==JNI_OK);
    puts("PASS real Lua binding: argument/error contracts, upload-buffer retry/lifetime, matching snapshots, two anchors, capabilities, stop/restart; all Lua/dmBuffer accesses on engine thread");
}
