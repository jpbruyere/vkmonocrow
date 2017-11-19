#ifndef CROW_H
#define CROW_H

#include <mono/jit/jit.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/object.h>
#include <mono/metadata/environment.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "crow_event.h"

#ifndef bool
#define bool uint8_t
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef struct Rectangle_t {
    int x,y,width,height;
}Rectangle;

volatile extern uint8_t* crowBuffer;
volatile extern uint32_t dirtyOffset;
volatile extern uint32_t dirtyLength;

void crow_init();

void crow_lock_update_mutex();
void crow_release_update_mutex();


#endif
