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

#define CROW_MOUSE_EVT      0b00000100
#define CROW_MOUSE_MOVE     0b00000100
#define CROW_MOUSE_UP       0b00000101
#define CROW_MOUSE_DOWN     0b00000110
#define CROW_MOUSE_WHEEL    0b00000111
#define CROW_KEY_EVT        0b00001000
#define CROW_KEY_UP         0b00001000
#define CROW_KEY_DOWN       0b00001001
#define CROW_KEY_PRESS      0b00001010
#define CROW_RESIZE         0b00010000

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

volatile extern uint32_t dirtyOffset;
volatile extern uint32_t dirtyLength;
volatile extern uint8_t* crowBuffer;

extern int MouseX;
extern int MouseY;
extern int Events;

void crow_init();
void crow_buffer_resize (uint32_t width, uint32_t height);
void crow_load ();
void crow_lock_update_mutex();
void crow_release_update_mutex();


#endif
