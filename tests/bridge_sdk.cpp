#include <dmsdk/sdk.h>
#include <new>
namespace bridge {
std::thread::id engine_thread;
bool fail_buffer=false;
int buffers_live=0;
static int BufferGC(lua_State* L){Engine();auto p=(dmBuffer::HBuffer*)luaL_checkudata(L,1,"buffer");delete *p;--buffers_live;return 0;}
static int Bytes(lua_State* L){Engine();auto p=(dmBuffer::HBuffer*)luaL_checkudata(L,1,"buffer");lua_pushlstring(L,(const char*)(*p)->bytes.data(),(*p)->bytes.size());return 1;}
static int Vector3(lua_State* L){dmScript::PushVector3(L,dmVMath::Vector3((float)luaL_optnumber(L,1,0),(float)luaL_optnumber(L,2,0),(float)luaL_optnumber(L,3,0)));return 1;}
static int Quat(lua_State* L){dmScript::PushQuat(L,dmVMath::Quat((float)luaL_optnumber(L,1,0),(float)luaL_optnumber(L,2,0),(float)luaL_optnumber(L,3,0),(float)luaL_optnumber(L,4,1)));return 1;}
static int VectorIndex(lua_State* L){const float* v=(const float*)lua_touserdata(L,1);const char* k=luaL_checkstring(L,2);int index=k[0]=='x'?0:k[0]=='y'?1:k[0]=='z'?2:3;lua_pushnumber(L,v[index]);return 1;}
void Register(lua_State* L){
    for(const char* name:{"vector3","vector4","quat","matrix4"}){luaL_newmetatable(L,name);lua_pushcfunction(L,VectorIndex);lua_setfield(L,-2,"__index");lua_pop(L,1);}
    luaL_newmetatable(L,"buffer");lua_pushcfunction(L,BufferGC);lua_setfield(L,-2,"__gc");lua_pop(L,1);
    lua_newtable(L);lua_pushcfunction(L,Bytes);lua_setfield(L,-2,"get_bytes");lua_setglobal(L,"buffer");
    lua_newtable(L);lua_pushcfunction(L,Vector3);lua_setfield(L,-2,"vector3");lua_pushcfunction(L,Quat);lua_setfield(L,-2,"quat");lua_setglobal(L,"vmath");
}
}
namespace dmScript {
template<class T>void Push(lua_State* L,const T& v,const char* type){bridge::Engine();T* out=(T*)lua_newuserdata(L,sizeof(T));new(out)T(v);luaL_getmetatable(L,type);lua_setmetatable(L,-2);}
void PushBuffer(lua_State* L,const LuaHBuffer& b){bridge::Engine();auto p=(dmBuffer::HBuffer*)lua_newuserdata(L,sizeof(dmBuffer::HBuffer));*p=b.b;luaL_getmetatable(L,"buffer");lua_setmetatable(L,-2);}
void PushVector3(lua_State* L,const dmVMath::Vector3& v){Push(L,v,"vector3");}
void PushVector4(lua_State* L,const dmVMath::Vector4& v){Push(L,v,"vector4");}
void PushQuat(lua_State* L,const dmVMath::Quat& v){Push(L,v,"quat");}
void PushMatrix4(lua_State* L,const dmVMath::Matrix4& v){Push(L,v,"matrix4");}
const dmVMath::Vector3* CheckVector3(lua_State* L,int i){bridge::Engine();return (const dmVMath::Vector3*)luaL_checkudata(L,i,"vector3");}
const dmVMath::Quat* CheckQuat(lua_State* L,int i){bridge::Engine();return (const dmVMath::Quat*)luaL_checkudata(L,i,"quat");}
}
