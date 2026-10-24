#pragma once
#include <stdint.h>
#include <vector>
#include <thread>
#include <assert.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
namespace bridge {
extern std::thread::id engine_thread;
extern bool fail_buffer;
extern int buffers_live;
inline void Engine() { assert(std::this_thread::get_id()==engine_thread); }
void Register(lua_State* L);
}
inline uint64_t dmHashString64(const char*) { bridge::Engine(); return 1; }
namespace dmVMath {
struct Vector3 { float x,y,z; Vector3(float a=0,float b=0,float c=0):x(a),y(b),z(c){} float getX()const{return x;}float getY()const{return y;}float getZ()const{return z;} };
struct Vector4 { float x,y,z,w;Vector4(float a=0,float b=0,float c=0,float d=0):x(a),y(b),z(c),w(d){} };
struct Quat { float x,y,z,w;Quat(float a=0,float b=0,float c=0,float d=1):x(a),y(b),z(c),w(d){} float getX()const{return x;}float getY()const{return y;}float getZ()const{return z;}float getW()const{return w;} };
struct Matrix4 { Vector4 columns[4];void setCol(int c,const Vector4& v){columns[c]=v;} };
}
namespace dmBuffer {
struct Buffer { std::vector<uint8_t> bytes; };
typedef Buffer* HBuffer;
enum Result { RESULT_OK, RESULT_ERROR };
enum ValueType { VALUE_TYPE_UINT8 };
struct StreamDeclaration { uint64_t name; ValueType type; uint32_t count; };
inline Result Create(uint32_t count,StreamDeclaration* streams,uint32_t,HBuffer* out){bridge::Engine();if(bridge::fail_buffer){bridge::fail_buffer=false;return RESULT_ERROR;}*out=new Buffer();(*out)->bytes.resize((size_t)count*streams[0].count);++bridge::buffers_live;return RESULT_OK;}
inline Result GetBytes(HBuffer b,void** data,uint32_t* size){bridge::Engine();*data=b->bytes.data();*size=(uint32_t)b->bytes.size();return RESULT_OK;}
inline void UpdateContentVersion(HBuffer){bridge::Engine();}
}
namespace dmScript {
enum Owner { OWNER_LUA };
struct LuaHBuffer { dmBuffer::HBuffer b; LuaHBuffer(dmBuffer::HBuffer p,Owner):b(p){} };
void PushBuffer(lua_State*,const LuaHBuffer&);
void PushVector3(lua_State*,const dmVMath::Vector3&);
void PushVector4(lua_State*,const dmVMath::Vector4&);
void PushQuat(lua_State*,const dmVMath::Quat&);
void PushMatrix4(lua_State*,const dmVMath::Matrix4&);
const dmVMath::Vector3* CheckVector3(lua_State*,int);
const dmVMath::Quat* CheckQuat(lua_State*,int);
inline int Ref(lua_State* L,int index){bridge::Engine();return luaL_ref(L,index);}
inline void Unref(lua_State* L,int index,int r){bridge::Engine();luaL_unref(L,index,r);}
}
namespace dmExtension {
enum Result { RESULT_OK };
enum EventType { EVENT_ID_ACTIVATEAPP,EVENT_ID_DEACTIVATEAPP };
struct Event { EventType m_Event; };
struct Params { lua_State* m_L; };
}
#define DM_DECLARE_EXTENSION(...)
