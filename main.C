/*
 *
 * File: main.C
 * Author: Saturnino Garcia (sat@cs)
 * Description: Cache simulator for 2 level cache
 *
 */

#include "cache.h"
#include "CPU.h"
#include "mem-sim.h"
#include "memQueue.h"
#include "prefetcher.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
	if(argc != 2) {
		printf("Usage: %s [trace file]\n",argv[0]);
		return 1;
	}

	int hitTimeL1 = 1; 
	int accessTimeL2 = 20; 
	int accessTimeMem = 50; 

	int lineSizeL1 = 16;
	int assocL1 = 2; 
	int totalSizeL1 = 32; 

	int lineSizeL2 = 32; 
	int assocL2 = 8; 
	int totalSizeL2 = 256; 

	int numSetsL1, numSetsL2;


	u_int32_t addr, cycles;
	u_int32_t runtime = 0; // number of cycles in runtime
	u_int32_t nonmemrt = 0; // number of cycles in runtime spent with non-mem instructions

	FILE *fp; // to write out the final stats

	// calc number of sets (if assoc == 0 then it's fully assoc so there is only 1 set)
	if(assocL1 != 0) numSetsL1 = totalSizeL1 * 1024 / (assocL1 * lineSizeL1);
	else {
		numSetsL1 = 1;
		assocL1 = totalSizeL1 * 1024 / lineSizeL1;
	}

	if(assocL2 != 0) numSetsL2 = totalSizeL2 * 1024 / (assocL2 * lineSizeL2);
	else {
		numSetsL2 = 1;
		assocL2 = totalSizeL2 * 1024 / lineSizeL2;
	}

	// D-cache is write through with no-write-alloc, LRU replacement
	Cache DCache(numSetsL1,assocL1,lineSizeL1,false,false,true);

	// L2 cache is writeback with write-alloc, LRU replacement
	Cache L2Cache(numSetsL2,assocL2,lineSizeL2,false,true,false);
	
	/**
	 * CPU_STATE = IDLE;
	 * OPENS TRACE FILE;
	 * CPU_DONE = FALSE;
	 * SETS CURRENT_REQUEST_FROM_cpu = TRUE;
	 * STARTS READING NEXT REQUEST.
	 */
	CPU cpu(argv[1]);

	/**
	 * NO CONSTRUCTOR CLASS FOR PF. THIS DOES NOTHING.
	 */
	Prefetcher pf;

	/**
	 * THIS IS NOTHING BUT A CIRCULAR QUEUE.
	 * ID = NAME['A','B','C']
	 *  CAPACITY = [10,20,30]
	 * SOURCE = [DCACHE, DCACHE, L2CACHE] the cache where we are receiving requests from; we need this for checking duplicate entries
	 * LATENCY = TIME TAKEN TO SERVICE A ACCESS REQUEST
	 * PIPELINED = IF THE GIVEN QUEUE IS PIPLENED OR NOT. TRUE IF IT IS PIPELINED.
	 * WRITE = // true = write buffer, false = read queue
	 * FRONT, REAR, SIZE = HEAD POINTERS
	 * QUEUE = STRUCT REQUEST THAT WE NEED TO GET A DATA OR ANYTHING. IMPLEMENTED USING CALLOC.
	 * READYTIME = TIME WHEN THE EARLIER REQUEST WILL BE READY
	 * TAG = TO STORE THE TAG OF A MEMORY BLOCK
	 * INDEX = TO STORE THE INDEX OF A MEMORY BLOCK.
	 */
	memQueue writeBuffer(10,&DCache,accessTimeL2,true,true,'a');
	memQueue queueL2(20,&DCache,accessTimeL2,true,false,'b');
	memQueue queueMem(10,&L2Cache,accessTimeMem,false,false,'c');


	// statistical stuff
	u_int32_t nRequestsL2 = 0; // number of requests sent out to L2 (both CPU and prefetcher requests)
	u_int32_t memCycles = 0; // number of cycles that main memory is being accessed
	u_int32_t memQsize = 0; // used for calculating average queue length

	u_int32_t curr_cycle = 1;
	Request req;
	bool isHit;

	/**
	 * This will run untill the end of the trace file.
	 */
	while(!cpu.isDone()) {

		isHit = false;

		/**
		 * Get the current status of the cpu. THis will change everytime depending upon the requests that we have.
		 */
		cpuState cpu_status = cpu.getStatus(curr_cycle);

//		printf("%u: %u\n",curr_cycle,cpu_status);

		if(cpu_status == READY) { // request is ready
			
			/**
			 * THis will just get the time stamp of the current process.
			 * This has all the data found in the CPU file.
			 * Req will have the current addr, pc, if the request is load or store, it the request is from CPU or not, this is issued at what time, and also if it is a hit or miss in L1 and L2. everything will be there in here.
			 */
			req = cpu.issueRequest(curr_cycle);

			/**
			 * This will traverse the whole L1 cache to see if it is hit or not. Returns True, if it is hit or false, if it is miss.
			 */
			// check for L1 hit
			isHit = DCache.check(req.addr,req.load);
			cpu.hitL1(isHit);	// to tell the status hit or miss for the current request in cpu.
			req.HitL1 = isHit;	// same as above.
			
			/**
			 * check if this is a hit or miss, and the current pc and memory address that was checked.
			 */
			// notify the prefetcher of what just happened with this memory op
			pf.cpuRequest(req);

			// Only go inside if this is a hit.
			if(isHit) {
				// if it is hit, then just get the current address inside the req.
				// this will again check for hit or miss in the Dcache. Also this will select a appropriate space in the cache and place it in it.
				DCache.access(req.addr,req.load);
				// change the state to idle. total access time will be the total cycles taken for the above set of instructions to finish.
				// this will fetch the next line from the trace file internally and you will get it from the issueRequest.
				cpu.completeRequest(curr_cycle);
			}
			// If the current memory request is not hit in the Dcache, check for the data in L2.
			// If the current request is a load go in there else add this to the queue.
			else if(req.load) {
				nRequestsL2++;
				// THis will add the current request in a circular buffer.
				if(queueL2.add(req,curr_cycle)) cpu.setStatus(WAITING); // CPU is now "waiting" for response from L2/mem
				else cpu.setStatus(STALLED_L2); // no room in l2 queue so we are "stalled" on this request
			}
			// iF the current request is not a load operation just add the request inside a write buffer.
			else { 
				nRequestsL2++;

				// If the current request is not in the L1 put it in the write buffer. and get the next request from the trace file.
				if (writeBuffer.add(req,curr_cycle)) cpu.completeRequest(curr_cycle); 
				else { // need to stall for an entry in the write buffer to open up
					cpu.setStatus(STALLED_WB);
				}
			}
		}
		//NEED TO UNDERSTAND THIS CODE IN MORE DEPTH.
		// PF can do some work if we are just waiting or idle OR if we had a hit in the D-cache so the D-to-L2 bus isn't needed
		else if(cpu_status == WAITING || cpu_status == IDLE || cpu_status == STALLED_WB || isHit) { // either waiting for lower mem levels or idle so PF can do something
			// THIS IS IF WE WANT TO FETCH A DATA FROM THE MEMORY INTO THE L1 CACHE OR L2 CACHE. 
			if(pf.hasRequest(curr_cycle)) { 
				nRequestsL2++;
				//DEPENDING UPON THE ALGORITHM THIS WILL GET THE ADDRESS FROM THE TRACE FILE.
				req = pf.getRequest(curr_cycle);
				req.fromCPU = false;
				req.load = true;
				// ADD THIS IN THE L2 QUEUE. NEED TO UNDERSTAND THE ELSE VERSION OF THE CODE.
				if(queueL2.add(req,curr_cycle)) pf.completeRequest(curr_cycle); // if added to queue then the request is "complete"
			}
			// IF THE ABOVE MENTIONED PREFETCHER IS NOT IN THE L2, PUT IT IN THE WRITE BACK BUFFER.
			if(cpu_status == STALLED_WB) { // attempt to put it in the write buffer
				req = cpu.getRequest(); // get the request we want

				if (writeBuffer.add(req,curr_cycle)) cpu.completeRequest(curr_cycle); // if added, we can move on
			}
		}
		else if(cpu_status == STALLED_L2) { // stalled b/c of L2 queue so let us just try this right away
			req = cpu.getRequest();
			if(queueL2.add(req,curr_cycle)) cpu.setStatus(WAITING); // l2 queue is free now so we can go into waiting state
		}

		// service the L2 queue
		if(queueL2.frontReady(curr_cycle)) { // check to see if the front element in the queue is ready
			//printf("servicing the l2 queue on cycle %u\n",curr_cycle);
			req = queueL2.getFront();

			isHit = L2Cache.check(req.addr,req.load);
			if (req.fromCPU)
				cpu.loadHitL2(isHit);

			if(isHit) {
				DCache.access(req.addr,req.load); // update D cache
				if(req.fromCPU) cpu.completeRequest(curr_cycle); // this request was from the CPU so update state to show we are done
				queueL2.remove(); // remove this request from the queue
			}
			else {
				if(queueMem.add(req,curr_cycle)) queueL2.remove(); // succesfully added to memory queue so we can remove it from L2 queue
			}
		}

		// service the memory queue
		if(queueMem.frontReady(curr_cycle)) {
			//printf("servicing the mem queue on cycle %u\n",curr_cycle);
			req = queueMem.getFront();
			queueMem.remove();

			// update both L2 and D cache
			L2Cache.access(req.addr,req.load);
			if(req.load) DCache.access(req.addr,req.load); // only update if this is a load

			if(req.fromCPU && req.load) cpu.completeRequest(curr_cycle);
		}

		// check to see if we are utilizing memory BW during this cycle
		if(queueMem.getSize() > 0) memCycles++;

		// used to find the average size of the memory queue
		memQsize += queueMem.getSize();

		// service the write buffer
		if(writeBuffer.frontReady(curr_cycle)) {
			req = writeBuffer.getFront();

			isHit = L2Cache.check(req.addr,req.load);
			cpu.storeHitL2(isHit);

			if(isHit) { // store hit in L2 so just save it and we are done
				L2Cache.access(req.addr,req.load);
				writeBuffer.remove();
			}
			else { // L2 is write-allocate so we need to load data from memory first
				if(queueMem.add(req,curr_cycle)) writeBuffer.remove(); // we can keep adding to the queue because we check for duplicates as part of add()
			}
		}

		curr_cycle++; // next cycle
	}

	curr_cycle--; // just for stats sake
	double avgMemQ = (double)memQsize / (double)curr_cycle;
	double L2BW = (double)nRequestsL2 / (double)curr_cycle;
	double memBW = (double)memCycles / (double)curr_cycle;

	fprintf(stderr, "total run time for %s trace: %u\n",argv[1], curr_cycle);
	fprintf(stderr, "D-cache total hit rate for %s trace: %f\n",argv[1],cpu.getHitRateL1());
	fprintf(stderr, "L2 cache total hit rate for %s trace: %f\n",argv[1],cpu.getHitRateL2());
	//rintf("%.4f\n",cpu.getAMAT());
	fprintf(stderr, "Average Memory Queue Size for %s trace: %f\n",argv[1],avgMemQ);
	fprintf(stderr, "L2 BW Utilization for %s trace : %f\n",argv[1],L2BW);
	fprintf(stderr, "Memory BW Utilization for %s trace: %f\n",argv[1],memBW);


	// create output file name based on trace file name
//	char* outfile = (char *)malloc(sizeof(char)*(strlen(argv[1])+5));
//	strcpy(outfile,argv[1]);
//	strcat(outfile,".out");

//	fp = fopen(outfile,"w"); // open outfile for writing
//fp = stdout;
//	free(outfile);

//	fprintf(fp,"%u\t",curr_cycle);
//	fprintf(fp,"%.4f\t",cpu.getHitRateL1());
//	fprintf(fp,"%.4f\t",cpu.getHitRateL2());
	printf("%.4f\t",cpu.getAMAT());
//	fprintf(fp,"%.4f\t",avgMemQ);
//	fprintf(fp,"%.4f\t",L2BW);
//	fprintf(fp,"%.4f\n",memBW);

//	char command[80];
//	sprintf(command, "cat /proc/%ld/rlimit",getpid());
//	system(command);

//	fclose(fp);

	return 0;
}

