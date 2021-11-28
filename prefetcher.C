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
// #define DEBUG

Prefetcher::Prefetcher() { _ready = false; }

bool Prefetcher::hasRequest(u_int32_t cycle) { return _ready; }

Request Prefetcher::getRequest(u_int32_t cycle) { return _nextReq; }

void Prefetcher::completeRequest(u_int32_t cycle) { _ready = false; }

void Prefetcher::cpuRequest(Request req) { 

	printf("pc: %x\t addr: %x\t stride: %x\t req.fromCPU: %d\t req.HitL1: %d\t cycle: %d\n", req.pc, req.addr, req.addr-prev.addr, req.fromCPU, req.HitL1, req.issuedAt);
	prev = req;
	_nextReq.addr=req.addr+32;
	_ready=true;
}

