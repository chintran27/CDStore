/*
 * BlockRingBuffer.hh
 * - Lock-free ringbuffer with blocking on empty/full
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
	volatile bool blockOnEmpty;
    volatile bool writerBlocked;
    volatile bool readerBlocked;
    pthread_mutex_t mAccess;
    pthread_cond_t cvEmpty;
    pthread_cond_t cvFull;

public:
   
	RingBuffer(int size, bool block=true, int wSyncMinCount=1) {
		fprintf(stderr, "BusyRingBuffer.hh is used\n");
		if (size < 2) size = 2; // shouldn't allow value less than 2.
		buffer = new Buffer_t[size];
		readIndex = 0; 
		writeIndex = 0;
		max = size;
		blockOnEmpty = block;
        writerBlocked = false;
        readerBlocked = false;
		pthread_mutex_init(&mAccess, NULL);
		pthread_cond_init(&cvEmpty, NULL);
		pthread_cond_init(&cvFull, NULL);
	}

	inline int nextVal(int x) { return (x+1) >= max ? 0 : x+1; }
	// inline int nextVal(int x) { return (x+1)%max; }

	int Insert(T* data, int len) {
		int nextWriteIndex = nextVal(writeIndex);
		if (nextWriteIndex == readIndex) {
            pthread_mutex_lock(&mAccess);
            writerBlocked = true;
		    while (nextWriteIndex == readIndex) {
                pthread_cond_wait(&cvFull, &mAccess);
            }
            writerBlocked = false;
            pthread_mutex_unlock(&mAccess);
		}
        buffer[writeIndex].len = len;
		memcpy(&(buffer[writeIndex].data), data, len);
		writeIndex = nextWriteIndex;
        if (readerBlocked) {
            pthread_mutex_lock(&mAccess);
            pthread_cond_signal(&cvEmpty);
            pthread_mutex_unlock(&mAccess);
        }
		return 0;
	}

	int Extract(T* data) {
        if (readIndex == writeIndex) { 
            pthread_mutex_lock(&mAccess);
            readerBlocked = true;
            while (readIndex == writeIndex) {
                if (!blockOnEmpty) {
                    pthread_mutex_unlock(&mAccess);
                    return -1;
                }
                pthread_cond_wait(&cvEmpty, &mAccess);
            }
            readerBlocked = false;
            pthread_mutex_unlock(&mAccess);
        }
		memcpy(data, &(buffer[readIndex].data), buffer[readIndex].len);
		readIndex = nextVal(readIndex);
        if (writerBlocked) {
            pthread_mutex_lock(&mAccess);
            pthread_cond_signal(&cvFull); 
            pthread_mutex_unlock(&mAccess);
        }
		return 0;
	}

	void PrintStats(char* text) {
	}	
};

#endif 
