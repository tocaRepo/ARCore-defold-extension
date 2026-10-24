#include <assert.h>
#include <stdio.h>
#include "../arcore/src/arcore.cpp"
int main(){
    bridge::engine_thread=std::this_thread::get_id();
    lua_State* L=luaL_newstate();luaL_openlibs(L);dmExtension::Params params={L};Initialize(&params);
    const char* code="local ok,e=arcore.start();assert(not ok and e.code=='unsupported_platform')\nlocal f=arcore.poll(720,1280);assert(not f.native and not f.tracking and not f.fresh and not f.complete and not f.pixels and f.status=='unsupported_platform')\nlocal c=arcore.get_capabilities();assert(not c.native and not c.initialized and c.runtime.depth=='unknown')\nfor _,fn in ipairs({arcore.hit_test,arcore.create_anchor,arcore.release_anchor}) do local ok,e=fn();assert(not ok and e.code=='unsupported_platform') end\nassert(arcore.stop());assert(arcore.stop())";
    if(luaL_dostring(L,code)){fprintf(stderr,"%s\n",lua_tostring(L,-1));return 1;}
    assert(lua_gettop(L)==0);Finalize(&params);lua_close(L);
    puts("PASS actual non-Android Lua import: structured unsupported result, no pixels or simulated tracking, repeatable stop");
}
