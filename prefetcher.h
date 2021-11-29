#ifndef PREFETCHER_H
#define PREFETCHER_H


#include <sys/types.h>
#include <math.h>
struct Request;





class Prefetcher {
	private:
	int head=-1, index_spt=-1, evict=0, capacity=0;
  public:
    Prefetcher();
	// should return true if a request is ready for this cycle
	bool hasRequest(u_int32_t cycle);

	// request a desired address be brought in
	Request getRequest(u_int32_t cycle);

	// this function is called whenever the last prefetcher request was successfully sent to the L2
	void completeRequest(u_int32_t cycle);

	u_int32_t getTag(u_int32_t addr);

	/*
	 * This function is called whenever the CPU references memory.
	 * Note that only the addr, pc, load, issuedAt, and HitL1 should be considered valid data
	 */
	void cpuRequest(Request req); 
};

#endif
