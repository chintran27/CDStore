/*
 * ExtendedQueue.hh
 */

#ifndef __EXTENDEDQUEUE_HH__
#define __EXTENDEDQUEUE_HH__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*macros for two special push and pop sizes*/
#define END_PUSH_SIZE 0
#define FULL_POP_SIZE -1

template <class T> class ExtendedQueue {
	private:
		T *queueBuffer_; 	

		int bytesPerElement_;	

		int maxSize_;	
		int currSize_;

		int pushIndex_;
		int popIndex_;

		int fullStat_;
		int endStat_;

		pthread_mutex_t queueLock_;
		pthread_cond_t pushCond_;
		pthread_cond_t popCond_;	
		pthread_cond_t fullCond_;	

	public:   
		ExtendedQueue(int maxSize) {
			maxSize_ = maxSize;
			if (maxSize_ <= 0) {
				fprintf(stderr, "Error: maxSize (%d) is invalid and should be a positive integer!\n", maxSize);				
				exit(1);
			}

			bytesPerElement_ = sizeof(T);
			queueBuffer_ = (T *) malloc(bytesPerElement_ * maxSize_);

			currSize_ = 0;
			pushIndex_ = 0;
			popIndex_ = 0;

			fullStat_ = 0;
			endStat_ = 0;

			/*initialize the mutex lock queueLock_*/	
			if (pthread_mutex_init(&queueLock_, NULL) != 0) {		
				fprintf(stderr, "Error: fail to initialize the mutex lock queueLock_!\n");		
				exit(1);		
			}

			/*initialize the condition pushCond_*/	
			if (pthread_cond_init(&pushCond_, NULL) != 0) {		
				fprintf(stderr, "Error: fail to initialize the condition pushCond_!\n");		
				exit(1);		
			}

			/*initialize the condition popCond_*/	
			if (pthread_cond_init(&popCond_, NULL) != 0) {		
				fprintf(stderr, "Error: fail to initialize the condition popCond_!\n");		
				exit(1);		
			}

			/*initialize the condition fullCond_*/	
			if (pthread_cond_init(&fullCond_, NULL) != 0) {		
				fprintf(stderr, "Error: fail to initialize the condition fullCond_!\n");		
				exit(1);		
			}

			fprintf(stderr, "\nAn ExtendedQueue has been constructed! \n");		
			fprintf(stderr, "Parameters: \n");			
			fprintf(stderr, "      maxSize_: %d \n", maxSize_);	
			fprintf(stderr, "\n");
		}

		~ExtendedQueue() {
			free(queueBuffer_);

			/*clean up the mutex lock queueLock_*/		
			pthread_mutex_destroy(&queueLock_);

			/*clean up the condition pushCond_*/		
			pthread_cond_destroy(&pushCond_);

			/*clean up the condition popCond_*/		
			pthread_cond_destroy(&popCond_);

			/*clean up the condition fullCond_*/		
			pthread_cond_destroy(&fullCond_);

			fprintf(stderr, "\nThe ExtendedQueue has been destructed! \n");	
			fprintf(stderr, "\n");
		}

		void push(T *pushBuffer, int pushSize = 1) {
			pthread_mutex_lock(&queueLock_);

			if (pushSize == END_PUSH_SIZE) {
				fullStat_ = 1;
				endStat_ = 1;

				pthread_cond_signal(&pushCond_);
				pthread_cond_signal(&fullCond_);
			}
			else {
				while (currSize_ + pushSize > maxSize_) {
					fullStat_ = 1;

					pthread_cond_signal(&fullCond_);

					pthread_cond_wait(&popCond_, &queueLock_);
				}

				if (pushIndex_ + pushSize <= maxSize_) {
					memcpy(queueBuffer_ + pushIndex_, pushBuffer, bytesPerElement_ * pushSize);
				}
				else {
					memcpy(queueBuffer_ + pushIndex_, pushBuffer, bytesPerElement_ * (maxSize_ - pushIndex_));
					memcpy(queueBuffer_, pushBuffer + (maxSize_ - pushIndex_), bytesPerElement_ * (pushSize - (maxSize_ - pushIndex_)));
				}

				pushIndex_ = (pushIndex_ + pushSize) < maxSize_ ? (pushIndex_ + pushSize) : ((pushIndex_ + pushSize) % maxSize_);

				currSize_ += pushSize;

				fullStat_ = 0;

				pthread_cond_signal(&pushCond_);
			}

			pthread_mutex_unlock(&queueLock_);
		}

		int pop(T *popBuffer, int popSize = 1) {	
			int empty = 0;

			pthread_mutex_lock(&queueLock_);				

			if (popSize == FULL_POP_SIZE) {
				while ((!fullStat_) && (!endStat_)) {
					pthread_cond_wait(&fullCond_, &queueLock_);
				}

				popSize = currSize_;
				empty = 1;
			}
			else {
				while ((popSize > currSize_) && (!endStat_)) {
					pthread_cond_wait(&pushCond_, &queueLock_);
				}

				if (endStat_) {
					if (popSize > currSize_) {
						popSize = currSize_;
					}
				}
			}			

			if (popIndex_ + popSize <= maxSize_) {
				memcpy(popBuffer, queueBuffer_ + popIndex_, bytesPerElement_ * popSize);
			}
			else {
				memcpy(popBuffer, queueBuffer_ + popIndex_, bytesPerElement_ * (maxSize_ - popIndex_));
				memcpy(popBuffer + (maxSize_ - popIndex_), queueBuffer_, bytesPerElement_ * (popSize - (maxSize_ - popIndex_)));
			}

			popIndex_ = (popIndex_ + popSize) < maxSize_ ? (popIndex_ + popSize) : ((popIndex_ + popSize) % maxSize_);

			currSize_ -= popSize;

			if (empty) {
				fullStat_ = 0;
			}

			if (!endStat_) {
				pthread_cond_signal(&popCond_);
			}

			pthread_mutex_unlock(&queueLock_);

			return popSize;
		}
};

#endif 
