#pragma once
#include <stdio.h>
#define dmLogInfo(...) do { printf(__VA_ARGS__); printf("\n"); } while(0)
#define dmLogError(...) do { fprintf(stderr,__VA_ARGS__); fprintf(stderr,"\n"); } while(0)
