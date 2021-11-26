#include "prefetcher.h"
#include "mem-sim.h"
#include <iostream>
#include <queue>
#include<math.h>
#include <stdio.h>
#include <sys/types.h>


// options to use for this define: [NO_PREFETCH, ALWAYS_PREFETCH, SAMPLE_PREFETCH, TAG_SEQ_PREFETCH, STRIDE_DIRECTED_PREFETCH]
#define TAG_SEQ_PREFETCH	
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




}
