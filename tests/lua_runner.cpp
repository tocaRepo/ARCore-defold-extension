extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <stdio.h>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    lua_State* L=luaL_newstate();luaL_openlibs(L);
    int result=luaL_dofile(L,argv[1]);if(result)fprintf(stderr,"%s\n",lua_tostring(L,-1));lua_close(L);return result?1:0;
}
