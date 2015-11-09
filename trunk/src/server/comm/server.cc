/*
 * server.cc
 */

#include "server.hh"
#include <string>
#include <string.h>
#include <sys/time.h>

DedupCore* dedupObj_;

using namespace std;

/*
 * constructor: initialize host socket
 *
 * @param port - port number
 * @param dedupObj - dedup object passed in
 *
 */
Server::Server(int port, DedupCore* dedupObj){
	//dedup. object
	dedupObj_ = dedupObj;

	//server port
	hostPort_ = port;

	//server socket initialization
	hostSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hostSock_ == -1){
		printf("Error initializing socket %d\n", errno);
	}

	//set socket options
	int *p_int = (int*)malloc(sizeof(int));
	*p_int = 1;

	if ((setsockopt(hostSock_, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int))==-1)||(setsockopt(hostSock_, SOL_SOCKET, SO_KEEPALIVE, (char*)p_int, sizeof(int)) == -1)){
		printf("Error setting options %d\n", errno);
		free(p_int);
	}
	free(p_int);

	//initialize address struct
	myAddr_.sin_family = AF_INET;
	myAddr_.sin_port = htons(hostPort_);

	memset(&(myAddr_.sin_zero),0,8);
	myAddr_.sin_addr.s_addr = INADDR_ANY;

	//bind port
	if(bind(hostSock_, (sockaddr*)&myAddr_, sizeof(myAddr_)) == -1){
		fprintf(stderr, "Error binding to socket %d\n", errno);
	}

	//start to listen
	if(listen(hostSock_, 10) == -1){
		fprintf(stderr, "Error listening %d\n", errno);
	}
}

void timerStart(double *t){
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*t = (double)tv.tv_sec+(double)tv.tv_usec*1e-6;
}

double timerSplit(const double *t){
	struct timeval tv;
	double cur_t;
	gettimeofday(&tv, NULL);
	cur_t = (double)tv.tv_sec + (double)tv.tv_usec*1e-6;
	return (cur_t - *t);		
}

/*
 * Thread function: each thread maintains a socket from a certain client
 *
 * @param lp - input parameter structure
 *
 */
void* SocketHandler(void* lp){
	//double timer,split,bw;
	
	//get socket from input param
	int *clientSock = (int*)lp;

	//variable initialization
	int bytecount;
	char * buffer = (char*)malloc(sizeof(char)*BUFFER_LEN);
	char * metaBuffer = (char *)malloc(sizeof(char)*META_LEN);
	bool* statusList = (bool*)malloc(sizeof(bool)*BUFFER_LEN);
	memset(statusList,0,sizeof(bool)*BUFFER_LEN);
	int metaSize;
	int user = 0;
	int dataSize = 0;
	//double first_total = 0;
	//double second_total = 0;
	

	//get user ID
	if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1){
		fprintf(stderr, "Error recv userID %d\n",errno);
	}
	user= ntohl(*(int*)buffer);

	memset(buffer, 0, BUFFER_LEN);
	int numOfShare = 0;

	//initialize hash object
	CryptoPrimitive* hashObj = new CryptoPrimitive(SHA256_TYPE);
	
	//main loop for recv data package
	while(true){

		/*recv indicator first*/
		if((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1){
			fprintf(stderr, "Error receiving data %d\n", errno);
		}

		/*if client closes, break loop*/
		if(bytecount == 0) break;

		int indicator = *(int*)buffer;

		/*recv following package size*/
		if((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1){
			fprintf(stderr, "Error receiving data %d\n", errno);
		}

		int packageSize = *(int*)buffer;
		int count = 0;
		
		/*recv following data*/
		while (count < packageSize){
			if((bytecount = recv(*clientSock, buffer+count, packageSize-count, 0)) == -1){
				fprintf(stderr, "Error receiving data %d\n", errno);
			}
			count += bytecount;
		}

		/*while metadata recv.ed, perform first stage deduplication*/
		if (indicator == META){
			memcpy(metaBuffer, buffer, count);
			metaSize = count;

			//timerStart(&timer);
			dedupObj_->firstStageDedup(user,(unsigned char*)metaBuffer, count, statusList, numOfShare, dataSize);
			//split = timerSplit(&timer);
			//first_total+= split;

			int ind = STAT;
			memcpy(buffer, &ind, sizeof(int));

			/*return the status list*/
			int bytecount;
			if ((bytecount = send(*clientSock, buffer, sizeof(int), 0)) == -1){
				fprintf(stderr, "Error sending data %d\n", errno);
			}

			memcpy(buffer,&numOfShare, sizeof(int));
			if ((bytecount = send(*clientSock, buffer, sizeof(int), 0)) == -1){
				fprintf(stderr, "Error sending data %d\n", errno);
			}

			if ((bytecount = send(*clientSock, statusList, sizeof(bool)*numOfShare, 0)) == -1){
				fprintf(stderr, "Error sending data %d\n", errno);
			}
		}
		
		/*while data recv.ed, perform second stage deduplication*/
		if(indicator == DATA){
			//timerStart(&timer);
			dedupObj_->secondStageDedup(user, (unsigned char*)metaBuffer, metaSize, statusList, (unsigned char*)buffer, hashObj);
			//split = timerSplit(&timer);
			//second_total+=split;
		}

		/*while download request recv.ed, perform restore*/
		if(indicator == DOWNLOAD){
			std::string fullFileName;
			fullFileName.assign(buffer, count);
			dedupObj_->restoreShareFile(user, fullFileName, 0, *clientSock, hashObj);

		}
	}

	//printf("%lf\t%lf\n",first_total, second_total);

	/*free objects*/
	delete hashObj;
	free(buffer);
	free(statusList);
	free(metaBuffer);
	free(clientSock);
	return 0;
}

/*
 * main procedure for receiving data
 * 
 */
void Server::runReceive(){
	addrSize_ = sizeof(sockaddr_in);

	//create a thread whenever a client connects
	while(true){
		printf("waiting for a connection\n");
		clientSock_ = (int*)malloc(sizeof(int));
		if((*clientSock_ = accept(hostSock_, (sockaddr*)&sadr_, &addrSize_))!= -1){
			printf("Received connection from %s\n", inet_ntoa(sadr_.sin_addr));
			pthread_create(&threadId_, 0, &SocketHandler, (void*)clientSock_);
			pthread_detach(threadId_);
		}
		else{
			fprintf(stderr, "Error accepting %d\n", errno);
		}
	}
}


