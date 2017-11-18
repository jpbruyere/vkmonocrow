#ifndef CROW_EVENT_H
#define CROW_EVENT_H

#include <stdbool.h>

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

typedef struct CrowEvent_t {
	uint32_t eventType;
	uint32_t data0;
	uint32_t data1;
	struct CrowEvent * pNext;
}CrowEvent;

CrowEvent* crow_evt_create(uint32_t eventType, uint32_t data0, uint32_t data1);
void crow_evt_enqueue(CrowEvent *pEvt);
CrowEvent* crow_evt_dequeue();
bool crow_evt_pending ();

#endif
