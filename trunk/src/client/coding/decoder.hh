/*
 *  encoder.hh 
 */

#ifndef __DECODER_HH__
#define __DECODER_HH__

#include "CDCodec.hh"
#include "BasicRingBuffer.hh"
#include "CryptoPrimitive.hh"

/* num of decoder threads */
#define DECODE_NUM_THREADS 2

/* ringbuffer size */
#define DECODE_RB_SIZE (2)

/* max secret size */
#define SECRET_SIZE (16*1024)

/* max share buffer size */
#define SHARE_BUFFER_SIZE (4*16*1024)

/* max write buffer size */
#define FWRITE_BUFFER_SIZE (4*1024*1024)

using namespace std;

class Decoder{
    public:
        /* thread parameter structure */
        typedef struct{
            int index;    // thread number
            Decoder* obj; // decoder object pointer
        }param_decoder;

        /* secret metadata structure */
        typedef struct{
            char data[SECRET_SIZE];
            int secretSize;
        }Secret_t;

        /* share metadata structure */
        typedef struct{
            char data[SHARE_BUFFER_SIZE];
            int secretSize;
            int shareSize;
        }ShareChunk_t;

        /* input share buffer */
        RingBuffer<ShareChunk_t>** inputbuffer_;

        /* output secret buffer */
        RingBuffer<Secret_t>** outputbuffer_;

        /* thread id array */
        pthread_t tid_[DECODE_NUM_THREADS+1];

        /* total number of secrets */
        int totalSecrets_;

        /* total number of clouds */
        int n_;

        /* output file pointer */
        FILE* fw_;

        /* share ID list pointer */
        int* kShareIDList_;

        /* crypto object array */
        CryptoPrimitive** cryptoObj_;

        /* decode object array */
        CDCodec* decodeObj_[DECODE_NUM_THREADS];

        /*
         * decoder constructor
         *
         * @param type - convergent dispersal type
         * @param n - total number of shares created from a secret
         * @param m - reliability degree
         * @param r - confidentiality degree
         * @param securetype - encryption and hash type
         */
        Decoder(int type, 
                int n, 
                int m, 
                int r, 
                int securetype);

        /*
         * destructor of decoder
         */
        ~Decoder();

        /*
         * set the total number of secrets we need to decode
         *
         * @param total - the total number of secrets
         */
        int setTotal(int totalSecrets);

        /*
         * set the file output pointer
         *
         * @param fp - the output file pointer
         */
        int setFilePointer(FILE* fp);

        /*
         * set the k shareID list
         *
         * @param list - the given kShareIDList
         */
        int setShareIDList(int* list);

        /*
         * add a share into particular ringbuffer
         *
         * @param item - the share object item
         * @param index - the index of targeting ringbuffer
         */
        int add(ShareChunk_t* item, int index);

        /*
         * test if it's the end of decoding a file
         *
         *
         */
        int indicateEnd();

        /*
         * thread handler for decode shares into secret
         *
         * @param param - input parameters for decode thread
         */
        static void* thread_handler(void* param);

        /*
         * collect thread for getting secret in order
         *
         * @param param - input parameters for collect thread
         */
        static void* collect(void* param);
};

#endif
