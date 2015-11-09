/*
 * ExtendedQueue test program
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "ExtendedQueue.hh"

#define MAX_QUEUE_SIZE 17
#define PUSH_SIZE 5
#define POP_SIZE FULL_POP_SIZE
//#define POP_SIZE 3

void* thread_push(void *arg) {
	int pushData[PUSH_SIZE];

	ExtendedQueue<int> *queueObj = (ExtendedQueue<int> *) arg;

	for (int i = 0; i < 1000; i++) {
		for (int j = 0; j < PUSH_SIZE; j++) pushData[j] = PUSH_SIZE * i + j;
		queueObj->push(pushData, PUSH_SIZE);

		printf("\n%d-th push: ", i);
		for (int j = 0; j < PUSH_SIZE; j++) printf("%d ", pushData[j]);
		printf("\n");
	}

	/*add an end indicator to tell thread_pop that all my pushes have been finished*/
	queueObj->push(pushData, END_PUSH_SIZE);

	printf("\n----------push end----------\n");
}

void* thread_pop(void *arg) {
	int popData[MAX_QUEUE_SIZE];
	int size;
	int cnt = 0;

	ExtendedQueue<int> *queueObj = (ExtendedQueue<int> *) arg;

	while (size = queueObj->pop(popData, POP_SIZE)) {
		printf("\n%d-th pop: ", cnt);
		for (int i = 0; i < size; i++) printf("%d ", popData[i]);
		printf("\n");

		cnt++;
	}

	printf("\n----------pop end----------\n");
}

int main(int argc, char *argv[]){	
	ExtendedQueue<int> *queueObj = new ExtendedQueue<int>(MAX_QUEUE_SIZE);	

	pthread_t tid1, tid2;

	if (pthread_create(&tid1, 0, thread_push, queueObj) != 0) {
		printf("fail to create thread_push\n");

		delete queueObj;

		return 0;	
	}

	if (pthread_create(&tid2, 0, thread_pop, queueObj) != 0) {
		printf("fail to create thread_pop\n");

		delete queueObj;

		return 0;	
	}

	sleep(600); 

	delete queueObj;

	return 0;	
}
