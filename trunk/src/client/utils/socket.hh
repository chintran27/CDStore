/*
 * socket.hh
 */

#ifndef __SOCKET_HH__
#define __SOCKET_HH__

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

/* action indicators */
#define SEND_META (-1)
#define SEND_DATA (-2)
#define GET_STAT (-3)
#define INIT_DOWNLOAD (-7)

using namespace std;

class Socket{
    private:
        /* port number */
        int hostPort_;

        /* ip address */
        char* hostName_;

        /* address structure */
        struct sockaddr_in myAddr_;

        /* host socket */
        int hostSock_;

    public:

        /*
         * constructor: initialize sock structure and connect
         *
         * @param ip - server ip address
         * @param port - port number
         */
        Socket(char *ip, int port, int userID);

        /*
         * @ destructor
         */
        ~Socket();

        /*
         * basic send function
         * 
         * @param raw - raw data buffer_
         * @param rawSize - size of raw data
         */
        int genericSend(char * raw, int rawSize);

        /*
         * metadata send function
         *
         * @param raw - raw data buffer_
         * @param rawSize - size of raw data
         *
         */
        int sendMeta(char * raw, int rawSize);

        /*
         * data send function
         *
         * @param raw - raw data buffer_
         * @param rawSize - size of raw data
         *
         */ 
        int sendData(char * raw, int rawSize); 

        /*
         * status recv function
         *
         * @param statusList - return int list
         * @param num - num of returned indicator
         *
         * @return statusList
         */
        int getStatus(bool * statusList, int* numOfShare); 

        /*
         * initiate downloading a file
         *
         * @param filename - the full name of the targeting file
         * @param namesize - the size of the file path
         *
         *
         */
        int initDownload(char * filename, int namesize);

        /*
         * download a chunk of data
         *
         * @param raw - the returned raw data chunk
         * @param retSize - the size of returned data chunk
         * @return raw 
         * @return retSize
         */ 
        int downloadChunk(char* raw, int* retSize);

        /*
         * data download function
         *
         * @param raw - raw data buffer
         * @param rawSize - the size of data to be downloaded
         * @return raw
         */
        int genericDownload(char *raw, int rawSize);
};

#endif
