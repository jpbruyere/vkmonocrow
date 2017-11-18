#include <pthread.h>
#include <stdint.h>

#include "crow_event.h"


static pthread_mutex_t crow_event_mutex = PTHREAD_MUTEX_INITIALIZER;
static CrowEvent * crow_evt_queue_start = NULL;
static CrowEvent * crow_evt_queue_end = NULL;

CrowEvent* crow_evt_create(uint32_t eventType, uint32_t data0, uint32_t data1){
	CrowEvent* pEvt = (CrowEvent*)malloc(sizeof(CrowEvent));
	pEvt->eventType = eventType;
	pEvt->data0 = data0;
	pEvt->data1 = data1;
	pEvt->pNext = NULL;
	return pEvt;
}

void crow_evt_enqueue(CrowEvent *pEvt){
	pthread_mutex_lock(&crow_event_mutex);
	if (crow_evt_queue_end){
		crow_evt_queue_end->pNext = pEvt;
		crow_evt_queue_end = pEvt;
	}else{
		crow_evt_queue_start = pEvt;
		crow_evt_queue_end = pEvt;
	}
	pthread_mutex_unlock(&crow_event_mutex);
}

CrowEvent* crow_evt_dequeue(){
	pthread_mutex_lock(&crow_event_mutex);
	CrowEvent* next = crow_evt_queue_start;
	if (next){
		if (crow_evt_queue_start->pNext)
			crow_evt_queue_start = crow_evt_queue_start->pNext;
		else
			crow_evt_queue_start = crow_evt_queue_end = NULL;
	}
	pthread_mutex_unlock(&crow_event_mutex);
	return next;
}

bool crow_evt_pending () {
	return (bool)(crow_evt_queue_start!=NULL);
}
