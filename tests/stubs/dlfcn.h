#pragma once
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define dlopen arcore_fixture_dlopen
#define dlsym arcore_fixture_dlsym
#define dlclose arcore_fixture_dlclose
#define dlerror arcore_fixture_dlerror
void* dlopen(const char*,int);
void* dlsym(void*,const char*);
int dlclose(void*);
const char* dlerror();
