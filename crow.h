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

#ifndef bool
#define bool uint8_t
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define CROW_MOUSE_MOVE		0x01
#define CROW_MOUSE_UP		0x02
#define CROW_MOUSE_DOWN		0x04

//extern pthread_mutex_t crow_resize_mutex;

typedef struct CrowEvent_t {
    uint32_t eventType;
    uint32_t data0;
    uint32_t data1;
    struct CrowEvent * pNext;
}CrowEvent;


CrowEvent* crow_create_evt(uint32_t eventType, uint32_t data0, uint32_t data1);
void crow_evt_enqueue(CrowEvent *pEvt);
CrowEvent* crow_evt_dequeue();
bool crow_evt_pending ();

typedef struct Rectangle_t {
    int x,y,width,height;
}Rectangle;

extern volatile uint32_t offsetY;
extern volatile uint32_t length;
extern volatile Rectangle dirtyRect;
extern volatile uint8_t* dirtyBmp;
extern volatile size_t dirtyBmpSize;

extern int MouseX;
extern int MouseY;
extern int Events;

void crow_init();
void crow_resize (int width, int height);
void crow_load ();
void crow_lock_update_mutex();
void crow_release_update_mutex();


#endif
