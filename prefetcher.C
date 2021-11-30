<<<<<<< HEAD
#include "prefetcher.h"
#include "mem-sim.h"
#include <iostream>
#include <queue>
#include<math.h>
#include <stdio.h>
#include <sys/types.h>
#include<iostream>
#include<queue>

using namespace std;

// options to use for this define: [NO_PREFETCH, ALWAYS_PREFETCH, SAMPLE_PREFETCH, TAG_SEQ_PREFETCH, STRIDE_DIRECTED_PREFETCH]
#define NO_PREFETCH	
// #define DEBUG

Request _nextReq;
Request previous;
#define BUFFER_MAX 20
Request req_buffer[BUFFER_MAX];

bool isfull=false;
int ptr=0;
bool _ready=false;
int buffer=0;

#ifdef TAG_SEQ_PREFETCH
#define MAXIMUM_ENTRIES 512
tag_pf tagged_sequential_prefetcher[MAXIMUM_ENTRIES];
int global_index=0;
#endif

#ifdef STRIDE_DIRECTED_PREFETCH
#define MAX_ENTRIES 512
struct spt_t{
	u_int32_t pc;
	u_int32_t addr;
	bool tag;
};
spt_t stride_prefetcher_buffer[MAX_ENTRIES];
int global_index=0, stride=0;
float spt_hit=0, spt_miss=0;
#endif

#ifdef RPT_PREFETCH
float correct_ind =0, total=0;
#define MAX_ENTRIES 512
enum STATE{ INITIAL,STEADY};
struct rpt_t{
	u_int32_t pc;
	u_int32_t addr;
	int32_t stride;
	u_int8_t state;
};
rpt_t rpt_prefetcher_buffer[MAX_ENTRIES];
bool correct=false; int head=-1, index_spt=-1;
#endif
Prefetcher::Prefetcher() { 
	_ready = false;
#ifdef TAG_SEQ_PREFETCH
	for(int i=0; i<MAXIMUM_ENTRIES; i++){
		tagged_sequential_prefetcher[i].tag=false;
	}
#endif

#ifdef STRIDE_DIRECTED_PREFETCH
	for(int i=0; i<MAX_ENTRIES; i++){
		stride_prefetcher_buffer[i].tag=false;
	}
#endif
	 }

bool Prefetcher::hasRequest(u_int32_t cycle) {
	// printf("has request called at: %d\t", cycle);
	
	return _ready;
}

Request Prefetcher::getRequest(u_int32_t cycle) {
	if(buffer==1){
	if(_ready==true && (ptr>=0 || ptr<=BUFFER_MAX)){
		_nextReq = req_buffer[ptr];
		if(ptr!=0)
			ptr--;
		isfull=false;
		
	}
	}
	return _nextReq;
}

void Prefetcher::completeRequest(u_int32_t cycle) {
	if(buffer==1){ 
	if(ptr==0){
		_ready=false;
	}
	}
	else _ready=false;
	return; }

u_int32_t Prefetcher::getTag(u_int32_t addr) {
	int b = (int)log2((double)_blockSize);
	int s = (int)log2((double)_numSets);
	int t = 32 - b - s;

	u_int32_t tag = addr >> (b + s);
	return tag;
}

/** One strange observation: #(nextReq.addr != req.addr) == #(_nextREq.addr == req.addr)
 * 
 */
void Prefetcher::cpuRequest(Request req) { 
#ifdef NO_PREFETCH
	if(!req.HitL1){
		buffer=1;
		if(isfull){
			return;
		}
		else{
			_ready=true;
			for(int i=0; i<BUFFER_MAX; i++){
				if(ptr>=BUFFER_MAX){
					isfull=true;
					return;
				}
				else{
					previous.addr = req.addr+32;
					req_buffer[ptr] =previous;
					ptr++;
					if(ptr==BUFFER_MAX)
						isfull=true;
				}
			}
		}
	}
	else{
		buffer=0;
		_nextReq.addr = req.addr+64;
		_ready=true;
	}
	return;
#endif

#ifdef RPT_PREFETCH
	if(head==-1){
		rpt_prefetcher_buffer[0].pc = req.pc;
		rpt_prefetcher_buffer[0].addr = req.addr;
		rpt_prefetcher_buffer[0].stride = 0;
		rpt_prefetcher_buffer[0].state = INITIAL;
		head=1;
		fprintf(stderr, "head: %d\n", head);
		return;
	}
	index_spt = -1;
	for(int i=0; i<MAX_ENTRIES; i++){
		if(req.pc == rpt_prefetcher_buffer[i].pc){
			index_spt = i;
			break;
		}
	}
	if(index_spt == -1){
		rpt_prefetcher_buffer[head%MAX_ENTRIES].pc = req.pc;
		rpt_prefetcher_buffer[head%MAX_ENTRIES].addr = req.addr;
		rpt_prefetcher_buffer[head%MAX_ENTRIES].stride = 0;
		rpt_prefetcher_buffer[head%MAX_ENTRIES].state = INITIAL;
		head++;
	}
	else{
		total++;
		
		correct = false;
		int stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
		if(rpt_prefetcher_buffer[index_spt].stride==stride){
			correct=true;
		}
		if((rpt_prefetcher_buffer[index_spt].state == INITIAL || rpt_prefetcher_buffer[index_spt].state ==STEADY) && (correct == true)){
			//rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
		
			rpt_prefetcher_buffer[index_spt].state = STEADY;
			rpt_prefetcher_buffer[index_spt].addr = req.addr;
			_nextReq.addr = rpt_prefetcher_buffer[index_spt].stride+req.addr;
			_ready=true;
		}
		else if((rpt_prefetcher_buffer[index_spt].state == STEADY || rpt_prefetcher_buffer[index_spt].state == INITIAL) && correct == false){
			fprintf(stderr, "Correct: %f", correct_ind/total);	
			correct_ind++;
			rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
			rpt_prefetcher_buffer[index_spt].addr = req.addr;
			rpt_prefetcher_buffer[index_spt].state = INITIAL;
			_ready=false;
		}
	}

	// 	else if((rpt_prefetcher_buffer[index_spt].state == INITIAL || rpt_prefetcher_buffer[index_spt].state == TRANSITION || rpt_prefetcher_buffer[index_spt].state == STEADY) && (correct==true)){
			
	// 		rpt_prefetcher_buffer[index_spt].addr = req.addr;
	// 		rpt_prefetcher_buffer[index_spt].state = STEADY;
	// 		_nextReq.addr = rpt_prefetcher_buffer[index_spt].stride+rpt_prefetcher_buffer[index_spt].addr;
	// 		_ready=true;
	// 	}
	// 	else if((rpt_prefetcher_buffer[index_spt].state == STEADY) && (correct==false)){
			
	// 		rpt_prefetcher_buffer[index_spt].addr = req.addr;
	// 		rpt_prefetcher_buffer[index_spt].state = INITIAL;
	// 		_nextReq.addr = rpt_prefetcher_buffer[index_spt].stride+rpt_prefetcher_buffer[index_spt].addr;
	// 		_ready=true;
	// 	}
	// 	else if((rpt_prefetcher_buffer[index_spt].state == TRANSITION) && (correct==false)){
	// 		rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
	// 		rpt_prefetcher_buffer[index_spt].addr = req.addr;
	// 		rpt_prefetcher_buffer[index_spt].state = NO_PREDICTION;
	// 	}
	// 	else if((rpt_prefetcher_buffer[index_spt].state == NO_PREDICTION) && (correct==true)){

	// 		rpt_prefetcher_buffer[index_spt].state = TRANSITION;
	// 		rpt_prefetcher_buffer[index_spt].addr = req.addr;
	// 					_nextReq.addr = rpt_prefetcher_buffer[index_spt].stride+rpt_prefetcher_buffer[index_spt].addr;
	// 		_ready=true;
	// 	}
	// 	else if((rpt_prefetcher_buffer[index_spt].state == NO_PREDICTION) && (correct==false)){
	// 		rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
	// 		rpt_prefetcher_buffer[index_spt].addr = req.addr;
	// 	}

	// }
	return;
#endif
	/**
	 * THIS IS THE CURRENT WINNER RIGHT NOW
	 * This is going to be the simplest implementation, I have ever done. Prefetch each and every block from the cache without checking for the miss or anything.
	 * [AVERAGE AMAT: 3.64 SECS]
	 */ 
#ifdef	ALWAYS_PREFETCH
	if(req.load){
	_nextReq.addr = req.addr + 32;
	_ready = true;
	}
#endif

	/**
	 * This is going to be the baseline. Sample Prefetcher given by the professor.
	 * [AVERAGE AMAT: 4.29 SECS] 25.644400
	 */
#ifdef SAMPLE_PREFETCH
	if(!_ready && !req.HitL1){
		_nextReq.addr = req.addr + 16;
		_ready=true;
	}

#endif


	/**
	 * This is the sequential tagged prefetcher algorithm!!!!!
	 * [AVERAGE AMAT:  3.766 SECS]
	 */
#ifdef TAG_SEQ_PREFETCH
	/**
	 * This loop is to check if we have an entry that has already being pre-fetch. If yes, change the bit the tag bit to 1 and get the next block.
	 */
	global_index = (req.addr) % MAXIMUM_ENTRIES;
	//printf("req.pc: %d	req.addr: %d\nreq.pc: %x	req.addr: %x\n", req.pc, req.addr,req.pc, req.addr);
	if(!req.fromCPU){
		tagged_sequential_prefetcher[global_index].addr = req.addr;
		tagged_sequential_prefetcher[global_index].tag = false;
		_ready=false;
		return;
	}
	else if(tagged_sequential_prefetcher[global_index].addr == req.addr){
		tagged_sequential_prefetcher[global_index].tag = true;
		_nextReq.addr = req.addr+64;
		_ready=true;
		return;
	}
	else{
		tagged_sequential_prefetcher[global_index].addr = req.addr;
		tagged_sequential_prefetcher[global_index].tag = true;
		_nextReq.addr = req.addr+32;
		_ready=true;
	}
	
	
	// for(int i=0; i<global_index%MAXIMUM_ENTRIES; i++){
	// 	if(req.addr == tagged_sequential_prefetcher[i].addr){
	// 		// we need to check if this particular block is still in the cache. This can be done by ishit?
	// 		if(!tagged_sequential_prefetcher[i].tag){
	// 			tagged_sequential_prefetcher[i].tag=false;
	// 			_nextReq.addr = req.addr + 32;
	// 			_ready=true;
	// 			return;
	// 		}
	// 		else{
	// 			_nextReq.addr = req.addr + 32;
	// 			_ready=true;
	// 			return;
	// 		}
	// 	}
	// }
	
	
	
	// // This will check if the block is prefetched. If yes, just get it in the buffer and set the tag to zero. There should be no prefetch for this. WE can have a prefetch if this is prefetched.
	// if(!req.fromCPU){
	// 	tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].addr = req.addr;
	// 	tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].tag = false;	
	// 	global_index++;
	// 	_ready=false;		
	// }
	// // If the req.addr is not the prefetched block, set the tag to one and prefetch the next block.
	// else{
	// 		tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].addr = req.addr;
	// 		tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].tag = true;
	// 		global_index++;
	// 		_nextReq.addr = req.addr + 32; 
	// 		_ready=true;
	// 	}

	// return; 
#endif

/**
 * Questions for stride prefetcher:
 * Correct Implementation of SPT table. This is not that effective for the traces presented here.
 * [AMAT: 4.09 SECS] TOTALSECS: 24.572100
 */
#ifdef STRIDE_DIRECTED_PREFETCH
	//check if we have the desired block of data. If not, put it in the buffer at the desired place.
	if(spt_hit!=0)
#ifdef DEBUG
		printf("spt_hit_ratio: %f\n", (spt_hit/(spt_hit+spt_miss)));
#endif

	if(req.fromCPU){

#ifdef DEBUG
		printf("prev.addr = %d\t prev.pc = %d\t req.pc = %d\t req.addr= %x\t stride: %d\n", previous.addr,previous.pc, req.pc, req.addr, req.addr-previous.addr);
#endif
		previous.addr = req.addr;
	}
	if(!_ready){
		global_index = (req.pc)%MAX_ENTRIES;

		if(stride_prefetcher_buffer[global_index].tag){
			if(stride_prefetcher_buffer[global_index].pc == req.pc){
				spt_hit++;
				stride = req.addr - stride_prefetcher_buffer[global_index].addr;
	#ifdef DEBUG
				printf("STRIDE: %d\t SPT_HIT: %f\n", stride, spt_hit);
	#endif
				if(stride!=0){
					_ready=true;
					_nextReq.addr = req.addr + stride;
					stride_prefetcher_buffer[global_index].addr = req.addr;
					
				}
				return;
			}
			else{
				spt_miss++;
				stride_prefetcher_buffer[global_index].addr = req.addr;
				stride_prefetcher_buffer[global_index].pc = req.pc;
				stride_prefetcher_buffer[global_index].tag = true;
				_ready=false;
			}
		}
		else{
			spt_miss++;
			stride_prefetcher_buffer[global_index].addr = req.addr;
			stride_prefetcher_buffer[global_index].pc = req.pc;
			stride_prefetcher_buffer[global_index].tag = true;
			_ready=false;
		}
	}
	return;
#endif




}
=======
#include "prefetcher.h"
#include "mem-sim.h"
#include <iostream>
#include <queue>
#include<math.h>
#include <stdio.h>
#include <sys/types.h>



// options to use for this define: [NO_PREFETCH, ALWAYS_PREFETCH, SAMPLE_PREFETCH, TAG_SEQ_PREFETCH, STRIDE_DIRECTED_PREFETCH, RPT_PREFETCH]
#define NO_PREFETCH	
// #define DEBUG

Request _nextReq;
Request prev;


bool _ready=false;

#ifdef NO_PREFETCH
#define MAX_ENTRIES 512
struct np_t{
	u_int32_t addr;
	int16_t stride;
};
np_t np_buffer[MAX_ENTRIES];
int head=-1, index_np=0;
#endif

#ifdef TAG_SEQ_PREFETCH
#define MAXIMUM_ENTRIES 512
tag_pf tagged_sequential_prefetcher[MAXIMUM_ENTRIES];
int global_index=0;
#endif

#ifdef STRIDE_DIRECTED_PREFETCH
int head=0, index_spt=0;
#define MAX_ENTRIES 512
struct spt_t{
	u_int32_t pc;
	u_int32_t addr;
	u_int16_t last_stride;
	bool tag;
};
spt_t stride_prefetcher_buffer[MAX_ENTRIES];
int global_index=0, stride=0;
float spt_hit=0, spt_miss=0;
#endif

#ifdef RPT_PREFETCH

#define MAX_ENTRIES 4096
enum STATE{ INITIAL, TRANSITION, NO_PREDICTION, STEADY, NO_ENTRY};
struct rpt_t{
	u_int32_t pc;
	u_int32_t prev_addr;
	int16_t stride;
	u_int8_t state;
};
rpt_t rpt_prefetcher_buffer[MAX_ENTRIES];
int global_index=0;
bool correct=false;
#endif 

Prefetcher::Prefetcher() { 
	_ready = false;
#ifdef TAG_SEQ_PREFETCH
	for(int i=0; i<MAXIMUM_ENTRIES; i++){
		tagged_sequential_prefetcher[i].tag=false;
	}
#endif

#ifdef STRIDE_DIRECTED_PREFETCH
	head=0; 
	index_spt=0;
	for(int i=0; i<MAX_ENTRIES; i++){
		stride_prefetcher_buffer[i].tag=false;
	}
#endif
#ifdef RPT_PREFETCH
	for(int i=0; i<MAX_ENTRIES; i++){
		rpt_prefetcher_buffer[i].state = NO_ENTRY;
	}
#endif
prev.addr=0;
	 }

bool Prefetcher::hasRequest(u_int32_t cycle) {
	return _ready;
}

Request Prefetcher::getRequest(u_int32_t cycle) {
return _nextReq;

}

void Prefetcher::completeRequest(u_int32_t cycle) { 
		_ready=false;
	return; }

/** One strange observation: #(nextReq.addr != req.addr) == #(_nextREq.addr == req.addr)
 * 
 */
void Prefetcher::cpuRequest(Request req) { 

#ifdef NO_PREFETCH
	if(head==-1){
		np_buffer[0].addr = req.addr;
		np_buffer[0].stride = 0;
		head=1;
		return;
	}
	int stride = req.addr - prev.addr;
	np_buffer[head%MAX_ENTRIES-1].stride = stride;
	index_np=-1;
	for(int i=0; i<MAX_ENTRIES; i++){
		if(req.addr == np_buffer[i].addr){
			_ready=true;
			index_np=i;
			_nextReq.addr = req.addr+np_buffer[i].stride;
		}
	}
	if(index_np==-1){
		np_buffer[head%MAX_ENTRIES].addr = req.addr;
		np_buffer[head%MAX_ENTRIES].stride = 0;
	}
	prev =req;

	// printf("Pc: %x\t Prev_addr: %x\t req.addr: %x\t stride: %d\n", req.pc, _nextReq.addr, req.addr, req.addr - _nextReq.addr);
	return;
#endif
	/**
	 * THIS IS THE CURRENT WINNER RIGHT NOW
	 * This is going to be the simplest implementation, I have ever done. Prefetch each and every block from the cache without checking for the miss or anything.
	 * [AVERAGE AMAT: 3.64 SECS] 21.8598
	 */ 
#ifdef	ALWAYS_PREFETCH
	if(req.load){
	_nextReq.addr = req.addr - 32;
	_ready = true;
	}
#endif

	/**
	 * This is going to be the baseline. Sample Prefetcher given by the professor.
	 * [AVERAGE AMAT: 4.29 SECS] 25.644400
	 */
#ifdef SAMPLE_PREFETCH
	if(!_ready && !req.HitL1){
		_nextReq.addr = req.addr + 16;
		_ready=true;
	}

#endif


	/**
	 * This is the sequential tagged prefetcher algorithm!!!!!
	 * [AVERAGE AMAT:  3.766 SECS]
	 */
#ifdef TAG_SEQ_PREFETCH
	/**
	 * This loop is to check if we have an entry that has already being pre-fetch. If yes, change the bit the tag bit to 1 and get the next block.
	 */
	global_index = (req.addr) % MAXIMUM_ENTRIES;
	//printf("req.pc: %d	req.addr: %d\nreq.pc: %x	req.addr: %x\n", req.pc, req.addr,req.pc, req.addr);
	if(!req.fromCPU){
		tagged_sequential_prefetcher[global_index].addr = req.addr;
		tagged_sequential_prefetcher[global_index].tag = false;
		_ready=false;
		return;
	}
	else if(tagged_sequential_prefetcher[global_index].addr == req.addr){
		tagged_sequential_prefetcher[global_index].tag = true;
		_nextReq.addr = req.addr+64;
		_ready=true;
		return;
	}
	else{
		tagged_sequential_prefetcher[global_index].addr = req.addr;
		tagged_sequential_prefetcher[global_index].tag = true;
		_nextReq.addr = req.addr+32;
		_ready=true;
	}
	
	
	// for(int i=0; i<global_index%MAXIMUM_ENTRIES; i++){
	// 	if(req.addr == tagged_sequential_prefetcher[i].addr){
	// 		// we need to check if this particular block is still in the cache. This can be done by ishit?
	// 		if(!tagged_sequential_prefetcher[i].tag){
	// 			tagged_sequential_prefetcher[i].tag=false;
	// 			_nextReq.addr = req.addr + 32;
	// 			_ready=true;
	// 			return;
	// 		}
	// 		else{
	// 			_nextReq.addr = req.addr + 32;
	// 			_ready=true;
	// 			return;
	// 		}
	// 	}
	// }
	
	
	
	// // This will check if the block is prefetched. If yes, just get it in the buffer and set the tag to zero. There should be no prefetch for this. WE can have a prefetch if this is prefetched.
	// if(!req.fromCPU){
	// 	tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].addr = req.addr;
	// 	tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].tag = false;	
	// 	global_index++;
	// 	_ready=false;		
	// }
	// // If the req.addr is not the prefetched block, set the tag to one and prefetch the next block.
	// else{
	// 		tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].addr = req.addr;
	// 		tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].tag = true;
	// 		global_index++;
	// 		_nextReq.addr = req.addr + 32; 
	// 		_ready=true;
	// 	}

	// return; 
#endif

/**
 * Questions for stride prefetcher:
 * Correct Implementation of SPT table. This is not that effective for the traces presented here.
 * [AMAT: 4.09 SECS] TOTALSECS: 24.572100
 */
#ifdef STRIDE_DIRECTED_PREFETCH
	//check if we have the desired block of data. If not, put it in the buffer at the desired place.
	if(spt_hit!=0)
#ifdef DEBUG
		printf("spt_hit_ratio: %f\n", (spt_hit/(spt_hit+spt_miss)));
#endif

	if(req.fromCPU){

#ifdef DEBUG
		printf("prev.addr = %d\t prev.pc = %d\t req.pc = %d\t req.addr= %x\t stride: %d\n", previous.addr,previous.pc, req.pc, req.addr, req.addr-previous.addr);
#endif
		previous.addr = req.addr;
	}
	if(!_ready){
		global_index = (req.pc)%MAX_ENTRIES;

		if(stride_prefetcher_buffer[global_index].tag){
			if(stride_prefetcher_buffer[global_index].pc == req.pc){
				spt_hit++;
				stride = req.addr - stride_prefetcher_buffer[global_index].addr;
				stride_prefetcher_buffer[global_index].addr = req.addr;
	#ifdef DEBUG
				printf("STRIDE: %d\t SPT_HIT: %f\n", stride, spt_hit);
	#endif
				if(stride!=0){
					_ready=true;
					_nextReq.addr = req.addr + 32;	
				}
				stride_prefetcher_buffer[global_index].last_stride = stride;
				return;
			}
			else{
				spt_miss++;
				stride_prefetcher_buffer[global_index].addr = req.addr;
				stride_prefetcher_buffer[global_index].pc = req.pc;
				stride_prefetcher_buffer[global_index].tag = true;
				_ready=false;
			}
		}
		else{
			spt_miss++;
			stride_prefetcher_buffer[global_index].addr = req.addr;
			stride_prefetcher_buffer[global_index].pc = req.pc;
			stride_prefetcher_buffer[global_index].tag = true;
			_ready=false;
		}
	}
	index_spt=0;
	 return;
#endif

#ifdef RPT_PREFETCH



// Get the global index indexed by the pc of the instruction.
global_index = (req.pc&0x0000ffff) % MAX_ENTRIES;
correct = false;
// Check if we have the same pc in the rpt table or it has been replaced by another pc due to aliasing or this is the first time we have seen this pc.
if(rpt_prefetcher_buffer[global_index].state == NO_ENTRY){
	rpt_prefetcher_buffer[global_index].pc = req.pc;
	rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
	rpt_prefetcher_buffer[global_index].stride = 0;
	rpt_prefetcher_buffer[global_index].state = INITIAL;
	_ready = false; // no prefetch as this will only tell to prefetch the current block, which doesn't make sense.
}

// We have an entry in the RPT! Wohooo!!. Let's see the state and check how is it going.
else{
	// Depending on the paper, if(prev_addr+stride = curr_addr) -> correct; else -> incorrect.
	if(req.addr == (rpt_prefetcher_buffer[global_index].prev_addr + rpt_prefetcher_buffer[global_index].stride))
		correct = true;
	// Change the state from INITIAL to TRANSITION and change the stride and everything.
	if((rpt_prefetcher_buffer[global_index].state == INITIAL) && (correct==true)){
		rpt_prefetcher_buffer[global_index].stride = req.addr - rpt_prefetcher_buffer[global_index].prev_addr;
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		rpt_prefetcher_buffer[global_index].state = TRANSITION;
		_ready=true;
		_nextReq.addr = req.addr+rpt_prefetcher_buffer[global_index].stride;
		return;
	}
	// Change the state from TRANS or STEADY to STEADY. We have a steady stride for 2 consecutive iteration.
	else if(( rpt_prefetcher_buffer[global_index].state == TRANSITION || rpt_prefetcher_buffer[global_index].state == STEADY) && (correct==true)){
		if(rpt_prefetcher_buffer[global_index].stride!=0){
		_ready=true;
		_nextReq.addr = req.addr+ rpt_prefetcher_buffer[global_index].stride;
		}
		else 
			_ready=false;
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		rpt_prefetcher_buffer[global_index].state = STEADY;
		//printf("ama vadhare hovu joie\n");
		
		
		return;
	}
	// If the stride changed from the steady state and hence change the state from STEADY to INITIAL.
	else if((rpt_prefetcher_buffer[global_index].state == STEADY) && (correct==false)){
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		rpt_prefetcher_buffer[global_index].state = INITIAL;
		_ready=false;
		// _nextReq.addr = req.addr+rpt_prefetcher_buffer[global_index].stride;
		return;
	}
	// If the stride changed in TRANSITION STATE: Go to the NO PRED State.
	else if((rpt_prefetcher_buffer[global_index].state == TRANSITION) && (correct==false)){
		rpt_prefetcher_buffer[global_index].stride = req.addr - rpt_prefetcher_buffer[global_index].prev_addr;
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		rpt_prefetcher_buffer[global_index].state = NO_PREDICTION;
		_ready=false;
		return;
	}
	// If the stride didn't changed in NO PRED State; change the state to TRANS.
	else if((rpt_prefetcher_buffer[global_index].state == NO_PREDICTION) && (correct==true)){
		if(rpt_prefetcher_buffer[global_index].stride!=0){
		_ready=true;
		_nextReq.addr = req.addr + rpt_prefetcher_buffer[global_index].stride;
		}
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		rpt_prefetcher_buffer[global_index].state = TRANSITION;
		
		return;
	}

	// If the stride chnaged in the NO PRED State, stay in the same state.
	else if((rpt_prefetcher_buffer[global_index].state == NO_PREDICTION) && (correct==false)){
		rpt_prefetcher_buffer[global_index].stride = req.addr - rpt_prefetcher_buffer[global_index].prev_addr;
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		_ready = false;
		return;
	}

	else{
	//	printf("aaiya kai b thai jaay aavu j na joie\n");
	}
	}

return;
#endif
}
>>>>>>> 6fe792374c40a7c8f101a676d341d22aba10b642
