#include "prefetcher.h"
#include "mem-sim.h"
#include <iostream>
#include <queue>
#include<math.h>
#include <stdio.h>
#include <sys/types.h>



// options to use for this define: [NO_PREFETCH, ALWAYS_PREFETCH, SAMPLE_PREFETCH, TAG_SEQ_PREFETCH, STRIDE_DIRECTED_PREFETCH, RPT_PREFETCH]
#define RPT_PREFETCH	
// #define DEBUG

Request _nextReq;
Request previous;


bool _ready=false;

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
queue<Request> MyQ;

#define MAX_ENTRIES 4096
enum STATE{ INITIAL, TRANSITION, NO_PREDICTION, STEADY};
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
	for(int i=0; i<MAX_ENTRIES; i++){
		stride_prefetcher_buffer[i].tag=false;
	}
#endif
	 }

bool Prefetcher::hasRequest(u_int32_t cycle) {
	return _ready;
}

Request Prefetcher::getRequest(u_int32_t cycle) {
	//Request req;
	return _nextReq;
}

void Prefetcher::completeRequest(u_int32_t cycle) { 
		_ready=false;
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
	_ready=false;
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

#ifdef RPT_PREFETCH

// To check 
if(_ready){
	//printf("aaiya thi pachu gayu\n");
	return;
}

// Get the global index indexed by the pc of the instruction.
global_index = (req.pc) % MAX_ENTRIES;
correct = false;
// Check if we have the same pc in the rpt table or it has been replaced by another pc due to aliasing or this is the first time we have seen this pc.
if(req.pc != rpt_prefetcher_buffer[global_index].pc){
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
	if((rpt_prefetcher_buffer[global_index].state == INITIAL) && (correct==false)){
		rpt_prefetcher_buffer[global_index].stride = req.addr - rpt_prefetcher_buffer[global_index].prev_addr;
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		rpt_prefetcher_buffer[global_index].state = TRANSITION;
		_ready=true;
		_nextReq.addr = req.addr+rpt_prefetcher_buffer[global_index].stride;
		return;
	}
	// Change the state from TRANS or STEADY to STEADY. We have a steady stride for 2 consecutive iteration.
	else if(( rpt_prefetcher_buffer[global_index].state == TRANSITION || rpt_prefetcher_buffer[global_index].state == STEADY) && (correct==true)){
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		rpt_prefetcher_buffer[global_index].state = STEADY;
		//printf("ama vadhare hovu joie\n");
		if(rpt_prefetcher_buffer[global_index].stride!=0){
		_ready=true;
		_nextReq.addr = req.addr+ 64;
		}
		else 
			_ready=false;
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
		rpt_prefetcher_buffer[global_index].prev_addr = req.addr;
		rpt_prefetcher_buffer[global_index].state = TRANSITION;
		_ready=true;
		_nextReq.addr = req.addr + 64;
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
