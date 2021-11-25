#include "prefetcher.h"
#include "mem-sim.h"

#include<math.h>
#include <stdio.h>
#include <sys/types.h>

// options to use for this define: [NO_PREFETCH, ALWAYS_PREFETCH, SAMPLE_PREFETCH, TAG_SEQ_PREFETCH, STRIDE_DIRECTED_PREFETCH]
#define STRIDE_DIRECTED_PREFETCH


Request _nextReq;
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
};
spt_t stride_prefetcher_buffer[MAX_ENTRIES];
int global_index=0, stride=0;
#endif



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

/** One strange observation: (nextReq.addr != req.addr) == (_nextREq.addr == req.addr)
 * 
 */
void Prefetcher::cpuRequest(Request req) { 
//	printf("Stride: %d\n",req.addr-_nextReq.addr);
//	_nextReq.addr = req.addr;

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
	 * [AVERAGE AMAT: 4.29 SECS]
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
	
	for(int i=0; i<global_index%MAXIMUM_ENTRIES; i++){
		if(req.addr == tagged_sequential_prefetcher[i].addr){
			// we need to check if this particular block is still in the cache. This can be done by ishit?
			if(!tagged_sequential_prefetcher[i].tag){
			//	printf("actually prefetched block got referenced.\n");
				tagged_sequential_prefetcher[i].tag=true;
				_nextReq.addr = req.addr + 32;
				_ready=true;
				return;
			}
			else{
				_nextReq.addr = req.addr + 32;
				_ready=true;
				return;
			}
		}
	}
	
	
	
	// This will check if the block is prefetched. If yes, just get it in the buffer and set the tag to zero. There should be no prefetch for this. WE can have a prefetch if this is prefetched.
	if(!req.fromCPU){
		tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].addr = req.addr;
		tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].tag = false;	
		global_index++;
		_ready=false;		
	}
	// If the req.addr is not the prefetched block, set the tag to one and prefetch the next block.
	else{
			tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].addr = req.addr;
			tagged_sequential_prefetcher[global_index%MAXIMUM_ENTRIES].tag = true;
			global_index++;
			_nextReq.addr = req.addr + 32; 
			_ready=true;
		}

	return; 
#endif

/**
 * Questions for stride prefetcher:
 * 1. How to get always a unique value for every unique program counter.
 * 2. if I use "%" this gives aleast two alias for the same pc.
 */
#ifdef STRIDE_DIRECTED_PREFETCH
	
	//global_index %= MAX_ENTRIES;
	//check if we have the desired block of data. If not, put it in the buffer at the desired place.
	if(!_ready)
		global_index = (req.pc) % MAX_ENTRIES;
		if(req.pc == stride_prefetcher_buffer[global_index].pc){
				stride = req.addr - stride_prefetcher_buffer[global_index].addr;
				if(stride!=0){
					_ready=true;
					_nextReq.addr = req.addr + stride;
					//stride_prefetcher_buffer[global_index].pc = req.pc;
					stride_prefetcher_buffer[global_index].addr = _nextReq.addr;
					//stride_prefetcher_buffer[global_index].valid = true;
					return;
				}
		}
		else{
				stride_prefetcher_buffer[global_index].addr = req.addr;
				stride_prefetcher_buffer[global_index].pc = req.pc;
				//stride_prefetcher_buffer[global_index].valid = true;
		}	
	return;
#endif


}
