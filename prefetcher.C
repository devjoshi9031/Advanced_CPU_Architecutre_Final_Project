#include "prefetcher.h"
#include "mem-sim.h"
#include <sys/types.h>

bool _ready=false;
Request _nextReq;
// options to use for this define: [NO_PREFETCH, ALWAYS_PREFETCH, SAMPLE_PREFETCH, TAG_SEQ_PREFETCH, STRIDE_DIRECTED_PREFETCH, RPT_PREFETCH]
#define RPT_PREFETCH	


// Define the states required in the RPT_PREFETCH ALGORITHM!!
#ifdef RPT_PREFETCH
// This is the total number of entries, I can have for RPT ENTRIES
#define MAX_ENTRIES 32
enum STATE{ INITIAL, TRANSITION, NO_PREDICTION, STEADY};
//
struct rpt_t{
	u_int32_t pc;		// 4B
	u_int32_t addr;		// 4B
	int16_t stride;		// 2B
	u_int8_t state;		// 1B
	u_int32_t count;	// 4B
};
rpt_t rpt_prefetcher_buffer[MAX_ENTRIES];
bool correct=false;
#endif

// Defines the states required for Tagged sequential prefetcher.
#ifdef TAG_SEQ_PREFETCH
#define MAXIMUM_ENTRIES 512
struct tag_pf{
	u_int32_t addr;	//4B
	bool tag;		//1B
	// bool valid;
	
};
int global_index=0;
tag_pf tagged_sequential_prefetcher[MAXIMUM_ENTRIES];
#endif

// Defines the states required for Stride prefetching.
#ifdef SPT_PREFETCH
#define MAX_ENTRIES 256
struct spt_t{
	u_int32_t pc, addr, stride; //12 bytes
	u_int32_t count;
};
spt_t stride_prefetcher_buffer[MAX_ENTRIES];
u_int32_t addr_buffer[256];
int sanity=0, buff_ptr=0;
#endif


	

Prefetcher::Prefetcher() { _ready = false; }

bool Prefetcher::hasRequest(u_int32_t cycle) { return _ready; }

Request Prefetcher::getRequest(u_int32_t cycle) { return _nextReq; }

void Prefetcher::completeRequest(u_int32_t cycle) { _ready = false; }

void Prefetcher::cpuRequest(Request req) { 

#ifdef SAMPLE_PREFETCH
		if(!_ready && !req.HitL1) {
		_nextReq.addr = req.addr + 16;
		_ready = true;
	}
#endif

#ifdef ALWAYS_PREFETCH
// If the current request is load, fetch the addr that is one block away from the current address.
	if(req.load){
		_nextReq.addr = req.addr+32;
		_ready=true;
	}
#endif


#ifdef TAG_SEQ_PREFETCH
	/**
	 * This loop is to check if we have an entry that has already being pre-fetch. If yes, change the bit the tag bit to 1 and get the next block.
	 */
	// Get the indexed by the pc, row.
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
#endif



#ifdef SPT_PREFETCH
	if(req.load){
		// THis will be the very first entry and after one iteration, never ever will be here.
	if(head==-1){
		stride_prefetcher_buffer[0].pc = req.pc;
		stride_prefetcher_buffer[0].addr = req.addr;
		stride_prefetcher_buffer[0].stride = 0;
		stride_prefetcher_buffer[0].count = req.issuedAt;
		_ready=true;
		_nextReq.addr = req.addr+32;
		head=1;

		return;
	}
	// just in caase if we cannot get a space in the buffer.
	index_spt = -1;
	// check the buffer if we have a entry corresponding to the pc. If yes, we will have that in index_spt
	for(int i=0; i<MAX_ENTRIES; i++){
		if(req.pc == stride_prefetcher_buffer[i].pc){
			index_spt = i;
			break;
		}
	}
	// entry not found!
	if(index_spt==-1){
		int min=2147483647;
		// check if capacity is not full, put the entry in the buffer.
	if(capacity==0){
		stride_prefetcher_buffer[head].pc = req.pc;
		stride_prefetcher_buffer[head].addr = req.addr;
		stride_prefetcher_buffer[head].stride = 0;
		stride_prefetcher_buffer[head].count = req.issuedAt;
		head++;
		if(head==MAX_ENTRIES){
			capacity=1;
		}
	}
	// if capacity == full, find the least recently used and evict that entry.
	else if(capacity==1){
		for(int i=0; i<MAX_ENTRIES; i++){
			if(stride_prefetcher_buffer[i].count < min){
				min=stride_prefetcher_buffer[i].count;
				evict = i;
			}
		}
		stride_prefetcher_buffer[evict].pc = req.pc;
		stride_prefetcher_buffer[evict].addr = req.addr;
		stride_prefetcher_buffer[evict].stride = 0;
		stride_prefetcher_buffer[evict].count = req.issuedAt;
		_ready=true;
		_nextReq.addr = req.addr+32;
	}
	}
	//if entry found in the table, calculate the stride and update the information in the table. If the stride is not equal to zero, fetch the block indexed by stride.
	else{
		stride_prefetcher_buffer[index_spt].stride = req.addr - stride_prefetcher_buffer[index_spt].addr;
		stride_prefetcher_buffer[index_spt].addr = req.addr;
		stride_prefetcher_buffer[index_spt].count= req.issuedAt;		
		if(stride_prefetcher_buffer[index_spt].stride >0){
			_ready=true;
			_nextReq.addr = req.addr+stride_prefetcher_buffer[index_spt].stride;
			return;
		}
	}
	}
	return;
#endif



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
		return;
	}
	// just in caase if we cannot get a space in the buffer.
	index_spt = -1;
	// check the buffer if we have a entry corresponding to the pc. If yes, we will have that in index_spt
	for(int i=0; i<MAX_ENTRIES; i++){
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
		// get the least recently used least recently used entry and evict that index to store the new one.
		for(int i=0; i<MAX_ENTRIES; i++){
			if(rpt_prefetcher_buffer[i].count < min){
				min=rpt_prefetcher_buffer[i].count;
				evict = i;
			}
		}
		rpt_prefetcher_buffer[evict].pc = req.pc;
		rpt_prefetcher_buffer[evict].addr = req.addr;
		rpt_prefetcher_buffer[evict].stride = 0;
		rpt_prefetcher_buffer[evict].count = req.issuedAt;
	}
	}
	else{
		correct=false;
		rpt_prefetcher_buffer[index_spt].count = req.issuedAt;
		// check if we have had the last stride correct or not.
		if(req.addr == (rpt_prefetcher_buffer[index_spt].addr+ rpt_prefetcher_buffer[index_spt].stride))
			correct = true;
		// If the state is initial and correct ==false
		if((rpt_prefetcher_buffer[index_spt].state == INITIAL) && (correct == false)){
				rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
				rpt_prefetcher_buffer[index_spt].addr = req.addr;
				rpt_prefetcher_buffer[index_spt].state = TRANSITION;
				
		}
		// If the state is initial, transition, or steady and the prev stride was correct, give confidence and change the state to STEADY. Now on prefetch with more confidence.
		else if ((rpt_prefetcher_buffer[index_spt].state == INITIAL || rpt_prefetcher_buffer[index_spt].state == TRANSITION || rpt_prefetcher_buffer[index_spt].state == STEADY) && (correct==true)){
			if(rpt_prefetcher_buffer[index_spt].stride!=0){
				_ready=true;
				_nextReq.addr = req.addr+rpt_prefetcher_buffer[index_spt].stride;
			}
			else{
				rpt_prefetcher_buffer[index_spt].addr = req.addr;
				rpt_prefetcher_buffer[index_spt].state = STEADY;
			}
			return;
		}
		// If the state is steady and the last stride was incorrect, punish the algorithm and set the state to initial.
		else if((rpt_prefetcher_buffer[index_spt].state == STEADY) && (correct==false)){
			rpt_prefetcher_buffer[index_spt].addr = req.addr;
			rpt_prefetcher_buffer[index_spt].state = INITIAL;
			_ready = true;
				_nextReq.addr = req.addr + rpt_prefetcher_buffer[index_spt].stride;
			return;
		}
		// If the state is transition and the last stride was incorrect, punish the algorithm and don't let it predict.
		else if((rpt_prefetcher_buffer[index_spt].state == TRANSITION) && (correct==false)){
			rpt_prefetcher_buffer[index_spt].stride = req.addr - rpt_prefetcher_buffer[index_spt].addr;
			rpt_prefetcher_buffer[index_spt].addr = req.addr;
			rpt_prefetcher_buffer[index_spt].state = NO_PREDICTION;
			_ready = true;
				_nextReq.addr = req.addr + rpt_prefetcher_buffer[index_spt].stride;
			return;
		}
		// If the state is no prediction and the last stride was correct, reward the stride and set state to transition.
		else if((rpt_prefetcher_buffer[index_spt].state == NO_PREDICTION) && (correct==true)){
			rpt_prefetcher_buffer[index_spt].addr = req.addr;
			rpt_prefetcher_buffer[index_spt].state = TRANSITION;
			if(rpt_prefetcher_buffer[index_spt].stride!=0){
				_ready = true;
				_nextReq.addr = req.addr + rpt_prefetcher_buffer[index_spt].stride;
			}
			return;
		}
		// If the state is transition and the last stride was incorrect, stay in the same state.
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


}

