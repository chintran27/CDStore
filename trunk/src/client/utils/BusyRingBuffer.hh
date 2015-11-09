/*
 * BusyRingBuffer.hh
 * - Ringbuffer based on busy waiting
 */

#ifndef __BusyRingBuffer_hh__
#define __BusyRingBuffer_hh__

#include <pthread.h>
#include <stdio.h>

#define barrier() __asm__ __volatile__("mfence": : :"memory")

template <class T> class RingBuffer {
    typedef struct {
        int len;
        T data;        
    } __attribute((packed)) Buffer_t; 
    Buffer_t* buffer; 
	volatile int readIndex;
	volatile int writeIndex;
	volatile int max;				// capacity of the buffer
	volatile int blockOnEmpty;

public:
   
	RingBuffer(int size, bool block=true, int wSyncMinCount=1) {
		fprintf(stderr, "BusyRingBuffer.hh is used\n");
		if (size < 2) size = 2; // shouldn't allow value less than 2.
		buffer = new Buffer_t[size];
		readIndex = 0; 
		writeIndex = 0;
		max = size;
		blockOnEmpty = block;
	}

	inline int nextVal(int x) { return (x+1) >= max ? 0 : x+1; }
	// inline int nextVal(int x) { return (x+1)%max; }

	int Insert(T* data, int len) {
		int nextWriteIndex = nextVal(writeIndex);
		while (nextWriteIndex == readIndex) {
		}
        buffer[writeIndex].len = len;
		memcpy(&(buffer[writeIndex].data), data, len);
		writeIndex = nextWriteIndex;
		return 0;
	}

	int Extract(T* data) {
		while (readIndex == writeIndex) {
			if (!blockOnEmpty) {
				return -1;
			}
		}
		memcpy(data, &(buffer[readIndex].data), buffer[readIndex].len);
		readIndex = nextVal(readIndex);
		return 0;
	}

	void PrintStats(char* text) {
	}	
};

#endif 
