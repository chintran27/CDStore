/*
 * downloader.hh
 */

#ifndef __DOWNLOADER_HH__
#define __DOWNLOADER_HH__

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <cstring>
#include <pthread.h>

/* downloader ringbuffer size */
#define DOWNLOAD_RB_SIZE 2048

/* downloader ringbuffer data max size */
#define RING_BUFFER_DATA_SIZE (16*1024)

/* downloader buffer size */
#define DOWNLOAD_BUFFER_SIZE (4*1024*1024)

/* length of hash 256 */
#define HASH_LENGTH 32

/* fingerprint size*/
#define FP_SIZE 32

#define MAX_NUMBER_OF_CLOUDS 16

/* number of download threads */
#define DOWNLOAD_NUM_THREADS 3


#include "BasicRingBuffer.hh"
#include "socket.hh"
#include "decoder.hh"
#include "CryptoPrimitive.hh"

using namespace std;

/*
 * download module
 * handle share to its targeting cloud
 *
 */
class Downloader{
    private:
        //prime number for compute hash
        long prime_;

        //total number of clouds
        int total_;

        //number of a subset of clouds
        int subset_;

    public:
        /* file metadata header structure */
        typedef struct{
            int fullNameSize; 
            long fileSize;
            int numOfPastSecrets;
            long sizeOfPastSecrets;
            int numOfComingSecrets;
            long sizeOfComingSecrets;
        }fileShareMDHead_t;

        /* share metadata header structure */
        typedef struct {
            unsigned char shareFP[FP_SIZE]; 
            int secretID;
            int secretSize;
            int shareSize;
        } shareMDEntry_t;

        /* file share count struct for download */
        typedef struct{
            long fileSize;
            int numOfShares;
        }shareFileHead_t;

        /* share detail struct for download */
        typedef struct{
            int secretID;
            int secretSize;
            int shareSize;
        }shareEntry_t;

        /* file header object structure for ringbuffer */
        typedef struct{
            shareFileHead_t file_header;
            char data[RING_BUFFER_DATA_SIZE];
        }fileHeaderObj_t;

        /* share header object structure for ringbuffer */
        typedef struct{
            shareEntry_t share_header;
            char data[RING_BUFFER_DATA_SIZE];
        }shareHeaderObj_t;

        /* union of objects for unifying ringbuffer objects */
        typedef struct{
            int type;
            union{
                fileHeaderObj_t fileObj;
                shareHeaderObj_t shareObj;
            };
        }Item_t;

        /* init object for initiating download */
        typedef struct{
            int type;
            char* filename;
            int namesize;
        }init_t;

        /* thread parameter structure */
        typedef struct{
            int cloudIndex;
            Downloader* obj;
        }param_t;

        /* file header pointer array for modifying header */
        fileShareMDHead_t ** headerArray_;

        /* socket array */
        Socket** socketArray_;

        /* metadata buffer */
        char ** downloadMetaBuffer_;

        /* container buffer */
        char ** downloadContainer_;

        /* size of file header */
        int fileMDHeadSize_;

        /* size of share header */
        int shareMDEntrySize_;

        /* thread id array */
        pthread_t tid_[DOWNLOAD_NUM_THREADS];

        /* decoder object pointer */
        Decoder* decodeObj_;

        /* signal buffer */
        RingBuffer<init_t>** signalBuffer_;

        /* download ringbuffer */
        RingBuffer<Item_t>** ringBuffer_;


        /*
         * constructor
         *
         * @param userID - ID of the user who initiate download
         * @param total - input total number of clouds
         * @param subset - input number of clouds to be chosen
         * @param obj - decoder pointer
         */
        Downloader(int total, int subset, int userID, Decoder* obj);

        /*
         * destructor
         *
         */
        ~Downloader();

        /*
         * test if it's the end of downloading a file
         *
         */
        int indicateEnd();

        /*
         * main procedure for downloading a file
         *
         * @param filename - targeting filename
         * @param namesize - size of filename
         * @param numOfCloud - number of clouds that we download data
         *
         */
        int downloadFile(char* filename, int namesize, int numOfCloud);	

        /*
         * downloader thread handler
         * 
         * @param param - input param structure
         *
         */
        static void* thread_handler(void* param);
};
#endif
