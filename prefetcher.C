#include "prefetcher.h"
#include "mem-sim.h"
#include <stdio.h>


Request _nextReq;
int numLoads;
int numStores;
bool _ready;


bool Prefetcher::hasRequest(u_int32_t cycle) {
	// if(_nextReq.addr==0)
	// 	return false;
	// else
	// 	return true;
	return _ready;
}

Request Prefetcher::getRequest(u_int32_t cycle) {
	//Request req;
	return _nextReq;
}

void Prefetcher::completeRequest(u_int32_t cycle) { 
	_ready=false;
	return; }

void Prefetcher::cpuRequest(Request req) { 
	//if(!_ready && !req.HitL1){
	if(req.load)
		numLoads++;		
	_nextReq.addr = req.addr + 64;
	_ready = true;
	//}
	{

		
	// 	printf("LIne issued at %d\t is the %dth load request\n", req.issuedAt, numLoads);
	// 	return;
	// }	
	// numStores++;
	// printf("Line issued at %d\t is the %dth store request\n", req.issuedAt, numStores);
	// //printf("%d\n", sizeof(tag_pf));
	return; }
