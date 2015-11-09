/*
 * server.hh
 */

#ifndef __SERVER_HH__
#define __SERVER_HH__

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#include "DedupCore.hh"
#include "BackendStorer.hh"

#define BUFFER_LEN (4*1024*1024)
#define META_LEN (2*1024*1024)
#define META (-1)
#define DATA (-2)
#define STAT (-3)
#define DOWNLOAD (-7)


using namespace std;

class Server{
private:

	//port number
	int hostPort_;

	//server address struct
	struct sockaddr_in myAddr_;
	
	//receiving socket
	int hostSock_;

	
	//socket size
	socklen_t addrSize_;

	//client socket
	int* clientSock_;

	//socket address
	struct sockaddr_in sadr_;
		
	//thread ID
	pthread_t threadId_;

public:
	Server(int port, DedupCore* dedupObj);
	void runReceive();
//	void* SocketHandler(void* lp);
};

#endif
