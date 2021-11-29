#include "prefetcher.h"
#include "mem-sim.h"
#include <iostream>
#include <queue>
#include<math.h>
#include <stdio.h>
#include <sys/types.h>


bool _ready=false;
Request _nextReq, prev;


// options to use for this define: [NO_PREFETCH, ALWAYS_PREFETCH, SAMPLE_PREFETCH, TAG_SEQ_PREFETCH, STRIDE_DIRECTED_PREFETCH, RPT_PREFETCH]
#define RPT_PREFETCH	

#ifdef SPT_PREFETCH
#define MAX_ENTRIES 512
struct spt_t{
	u_int32_t pc, addr, stride;
	u_int8_t count;
};
spt_t stride_prefetcher_buffer[512];
#endif

#ifdef TAG_SEQ_PREFETCH
struct tag_pf{
	u_int32_t addr;
	bool tag;
	// bool valid;
};
#define MAXIMUM_ENTRIES 512
tag_pf tagged_sequential_prefetcher[MAXIMUM_ENTRIES];

#endif

#ifdef RPT_PREFETCH
#define MAX_ENTRIES 4096
enum STATE{ INITIAL, TRANSITION, NO_PREDICTION, STEADY, NO_ENTRY};
struct rpt_t{
	u_int32_t pc;
	u_int32_t addr;
	int16_t stride;
	u_int8_t state;
	u_int32_t count;
};
rpt_t rpt_prefetcher_buffer[MAX_ENTRIES];
bool correct=false;
#endif


Prefetcher::Prefetcher() { _ready = false; }

bool Prefetcher::hasRequest(u_int32_t cycle) { return _ready; }

Request Prefetcher::getRequest(u_int32_t cycle) { return _nextReq; }

void Prefetcher::completeRequest(u_int32_t cycle) { _ready = false; }

void Prefetcher::cpuRequest(Request req) { 

#ifdef RPT_PREFETCH
if(req.load){
// THis will be the very first entry and after one iteration, never ever will be here.
	if(head==-1){
		rpt_prefetcher_buffer[0].pc = req.pc;
		rpt_prefetcher_buffer[0].addr = req.addr;
		rpt_prefetcher_buffer[0].stride = 0;
		rpt_prefetcher_buffer[0].state = INITIAL;
		rpt_prefetcher_buffer[0].count = req.issuedAt;
		head=1;
		fprintf(stderr, "head: %d\n", head);
		return;
	}
	// just in caase if we cannot get a space in the buffer.
	index_spt = -1;
	// check the buffer if we have a entry corresponding to the pc. If yes, we will have that in index_spt
	for(int i=0; i<512; i++){
		if(req.pc == rpt_prefetcher_buffer[i].pc){
			index_spt = i;
			break;
		}
	}

	// If we can't find a space in the buffer, we need to put this entry in the buffer.
	if(index_spt==-1){
		// This is to count the minimum count for lru.
		int min=2147483647;
	// if the buffer is not full, place the entry on the head row.
	if(capacity==0){
		rpt_prefetcher_buffer[head].pc = req.pc;
		rpt_prefetcher_buffer[head].addr = req.addr;
		rpt_prefetcher_buffer[head].stride = 0;
		rpt_prefetcher_buffer[head].state = INITIAL;
		rpt_prefetcher_buffer[head].count = req.issuedAt;
		head++;
		// if the head variable is more than MAX_ENTRIES indicate capacity is full.
		if(head==MAX_ENTRIES)
			capacity=1;
	}

	else if(capacity==1){
		// get the least recently used least recently used 
		for(int i=0; i<512; i++){
			if(rpt_prefetcher_buffer[i].count < min){
				min=rpt_prefetcher_buffer[i].count;
				evict = i;
			}
		}
		rpt_prefetcher_buffer[evict].pc = req.pc;
		rpt_prefetcher_buffer[evict].addr = req.addr;
		rpt_prefetcher_buffer[evict].stride = 0;
		rpt_prefetcher_buffer[evict].count = req.issuedAt;
		_ready=true;
		_nextReq.addr = req.addr+32;
	}
	else
		printf("aaiya aavu j na joie\n");
	}
	else{
		correct=false;
		rpt_prefetcher_buffer[index_spt].count = req.issuedAt;
		if(req.addr == (rpt_prefetcher_buffer[index_spt].addr+ rpt_prefetcher_buffer[index_spt].stride))
			correct = true;
		if((rpt_prefetcher_buffer[index_spt].state == INITIAL) && (correct == true)){
				rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
				rpt_prefetcher_buffer[index_spt].addr = req.addr;
				rpt_prefetcher_buffer[index_spt].state = TRANSITION;
				//add prefetch here!
		}
		else if ((rpt_prefetcher_buffer[index_spt].state == TRANSITION || rpt_prefetcher_buffer[index_spt].state == STEADY) && (correct==true)){
			if(rpt_prefetcher_buffer[index_spt].stride!=0){
				_ready=true;
				_nextReq.addr = req.addr+32;
			}
			else{
				_ready=false;
				rpt_prefetcher_buffer[index_spt].addr = req.addr;
				rpt_prefetcher_buffer[index_spt].state = STEADY;
			}
			return;
		}
		else if((rpt_prefetcher_buffer[index_spt].state == STEADY) && (correct==false)){
			rpt_prefetcher_buffer[index_spt].addr = req.addr;
			rpt_prefetcher_buffer[index_spt].state = INITIAL;
			_ready =false;
			return;
		}
		else if((rpt_prefetcher_buffer[index_spt].state == TRANSITION) && (correct==false)){
			rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
			rpt_prefetcher_buffer[index_spt].addr = req.addr;
			rpt_prefetcher_buffer[index_spt].state = NO_PREDICTION;
			_ready=false;
			return;
		}
		else if((rpt_prefetcher_buffer[index_spt].state == NO_PREDICTION) && (correct==true)){
			rpt_prefetcher_buffer[index_spt].addr = prev.addr;
			rpt_prefetcher_buffer[index_spt].state = TRANSITION;
			if(rpt_prefetcher_buffer[index_spt].stride!=0){
				_ready = true;
				_nextReq.addr = req.addr + 32;
			}
			return;
		}
		else if ((rpt_prefetcher_buffer[index_spt].state == NO_PREDICTION) && (correct==false)){
			rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
			rpt_prefetcher_buffer[index_spt].addr = req.addr;
			_ready = false;
			return;
		}
	}
}
return;
#endif


#ifdef SPT_PREFETCH
	if(req.load){
	if(head==-1){
		stride_prefetcher_buffer[0].pc = req.pc;
		stride_prefetcher_buffer[0].addr = req.addr;
		stride_prefetcher_buffer[0].stride = 0;
		stride_prefetcher_buffer[0].count = 0;
		_ready=true;
		_nextReq.addr = req.addr+32;
		head=1;
		fprintf(stderr, "head: %d\n", head);
		return;
	}

	index_spt = -1;
	for(int i=0; i<512; i++){
		if(req.pc == stride_prefetcher_buffer[i].pc){
			index_spt = i;
			break;
		}
	}
	if(index_spt==-1){
		int min=2147483647;
	if(capacity==0){
		stride_prefetcher_buffer[head].pc = req.pc;
		stride_prefetcher_buffer[head].addr = req.addr;
		stride_prefetcher_buffer[head].stride = 0;
		stride_prefetcher_buffer[head].count = 0;
		_ready=true;
		_nextReq.addr = req.addr+32;
		head++;
		if(head==MAX_ENTRIES){
			capacity=1;
		}
	}
	else if(capacity==1){
		for(int i=0; i<512; i++){
			if(stride_prefetcher_buffer[i].count < min){
				min=stride_prefetcher_buffer[i].count;
				evict = i;
			}
		}
		stride_prefetcher_buffer[evict].pc = req.pc;
		stride_prefetcher_buffer[evict].addr = req.addr;
		stride_prefetcher_buffer[evict].stride = 0;
		stride_prefetcher_buffer[evict].count = 0;
		_ready=true;
		_nextReq.addr = req.addr+32;
	}
	else
		printf("aaiya aavu j na joie\n");
	}
	else{
		stride_prefetcher_buffer[index_spt].stride = req.addr - stride_prefetcher_buffer[index_spt].addr;
		stride_prefetcher_buffer[index_spt].addr = req.addr;
		stride_prefetcher_buffer[index_spt].count++;
		// fprintf(stderr, "Head: %d\t count: %d\n", head, stride_prefetcher_buffer[index_spt].count++);		
		if(stride_prefetcher_buffer[index_spt].stride !=0){
			_ready=true;
			_nextReq.addr = req.addr+32;
			return;
		}
	}
	}
	return;
#endif
}

