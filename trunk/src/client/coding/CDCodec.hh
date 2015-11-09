/*
 * CDCodec.hh
 */

#ifndef __CDCODEC_HH__
#define __CDCODEC_HH__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/*for the use of CryptoPrimitive*/
#include "CryptoPrimitive.hh"

/*for the use of gf_t object*/
extern "C" {
#include "gf_complete.h"
}

/*macro for the type of CRSSS*/
#define CRSSS_TYPE 0
/*macro for the type of AONT-RS*/
#define AONT_RS_TYPE 1
/*macro for the type of old CAONT-RS*/
#define OLD_CAONT_RS_TYPE 2
/*macro for the type of CAONT-RS*/
#define CAONT_RS_TYPE 3

#define MAX_SECRET_SIZE (64<<10)

using namespace std;

class CDCodec{
    private:
        /*convergent dispersal type*/
        int CDType_; 

        /*total number of shares generated from a secret*/
        int n_;
        /*reliability degree (i.e. maximum number of lost shares that can be tolerated)*/
        int m_;
        /*minimum number of shares for reconstructing the original secret*/
        int k_;
        /*confidentiality degree (i.e. maximum number of shares from which nothing can be derived)*/
        int r_;

        /*variables for hash generation and data encryption*/
        CryptoPrimitive *cryptoObj_;

        /*number of bytes per secret word*/
        int bytesPerSecretWord_;

        /*variables used in hash generation specially in CRSSS*/
        int secretWordsPerGroup_;
        int bytesPerGroup_;

        /*variables used in encryption specially in CAONT-RS*/
        unsigned char *key_;

        /*variables for generating and storing r hashes from each group of secret words specially in CRSSS*/
        unsigned char *hashInputBuffer_;
        unsigned char *rHashes_;

        /*a buffer for storing the aligned secret and its size*/
        int alignedSecretBufferSize_;
        unsigned char *alignedSecretBuffer_;

        /*a word of size bytesPerSecretWord_ for storing an index specially in old CAONT-RS*/
        unsigned char *wordForIndex_;

        /*a constant block of size alignedSecretBufferSize_ specially in CAONT-RS*/
        unsigned char *alignedSizeConstant_;

        /*a buffer for storing the data before erasure coding and its size*/
        int erasureCodingDataSize_;
        unsigned char *erasureCodingData_;

        /*number of bits per GF word*/
        int bitsPerGFWord_;
        /*gf_t object for accelerating GF calculation*/
        gf_t gfObj_;

        /*the distribution matrix of an erasure code (IDA or RS)*/
        int *distributionMatrix_;

        /*two k * k matrices for decoding*/
        int *squareMatrix_;
        int *inverseMatrix_;

        /*
         * invert the square matrix squareMatrix_ into inverseMatrix_ in GF
         *
         * @return - a boolean value that indicates if the square matrix squareMatrix_ is invertible
         */
        bool squareMatrixInverting();

        /*
         * encode a secret into n shares using CRSSS
         *
         * @param secretBuffer - a buffer that stores the secret
         * @param secretSize - the size of the secret
         * @param shareBuffer - a buffer for storing the n generated shares <return>
         * @param shareSize - the size of each share <return>
         *
         * @return - a boolean value that indicates if the encoding succeeds
         */
        bool crsssEncoding(unsigned char *secretBuffer, int secretSize, unsigned char *shareBuffer, int *shareSize);

        /*
         * decode the secret from k = n - m shares using CRSSS
         *
         * @param shareBuffer - a buffer that stores the k shares 
         * @param kShareIDList - a list that stores the IDs of the k shares
         * @param shareSize - the size of each share 
         * @param secretSize - the size of the secret
         * @param secretBuffer - a buffer for storing the secret <return>
         *
         * @return - a boolean value that indicates if the decoding succeeds
         */
        bool crsssDecoding(unsigned char * shareBuffer, int *kShareIDList, int shareSize, int secretSize, unsigned char *secretBuffer);

        /*
         * encode a secret into n shares using AONT-RS (proposed by Jason K. Resch and James S. Plank)
         *
         * @param secretBuffer - a buffer that stores the secret
         * @param secretSize - the size of the secret
         * @param shareBuffer - a buffer for storing the n generated shares <return>
         * @param shareSize - the size of each share <return>
         *
         * @return - a boolean value that indicates if the encoding succeeds
         */
        bool aontRSEncoding(unsigned char *secretBuffer, int secretSize, 
                unsigned char *shareBuffer, int *shareSize);

        /*
         * decode the secret from k = n - m shares using AONT-RS (proposed by Jason K. Resch and James S. Plank)
         *
         * @param shareBuffer - a buffer that stores the k shares 
         * @param kShareIDList - a list that stores the IDs of the k shares
         * @param shareSize - the size of each share 
         * @param secretSize - the size of the secret
         * @param secretBuffer - a buffer for storing the secret <return>
         *
         * @return - a boolean value that indicates if the decoding succeeds
         */
        bool aontRSDecoding(unsigned char * shareBuffer, int *kShareIDList, int shareSize, 
                int secretSize, unsigned char *secretBuffer);

        /*
         * encode a secret into n shares using old CAONT-RS (proposed in the HotStorage '14 paper)
         *
         * @param secretBuffer - a buffer that stores the secret
         * @param secretSize - the size of the secret
         * @param shareBuffer - a buffer for storing the n generated shares <return>
         * @param shareSize - the size of each share <return>
         *
         * @return - a boolean value that indicates if the encoding succeeds
         */
        bool caontRSOldEncoding(unsigned char *secretBuffer, int secretSize, unsigned char *shareBuffer, int *shareSize);

        /*
         * decode the secret from k = n - m shares using old CAONT-RS (proposed in the HotStorage '14 paper)
         *
         * @param shareBuffer - a buffer that stores the k shares 
         * @param kShareIDList - a list that stores the IDs of the k shares
         * @param shareSize - the size of each share 
         * @param secretSize - the size of the secret
         * @param secretBuffer - a buffer for storing the secret <return>
         *
         * @return - a boolean value that indicates if the decoding succeeds
         */
        bool caontRSOldDecoding(unsigned char * shareBuffer, int *kShareIDList, int shareSize, int secretSize, unsigned char *secretBuffer);

        /*
         * encode a secret into n shares using CAONT-RS
         *
         * @param secretBuffer - a buffer that stores the secret
         * @param secretSize - the size of the secret
         * @param shareBuffer - a buffer for storing the n generated shares <return>
         * @param shareSize - the size of each share <return>
         *
         * @return - a boolean value that indicates if the encoding succeeds
         */
        bool caontRSEncoding(unsigned char *secretBuffer, int secretSize, unsigned char *shareBuffer, int *shareSize);	

        /*
         * decode the secret from k = n - m shares using CAONT-RS
         *
         * @param shareBuffer - a buffer that stores the k shares 
         * @param kShareIDList - a list that stores the IDs of the k shares
         * @param shareSize - the size of each share 
         * @param secretSize - the size of the secret
         * @param secretBuffer - a buffer for storing the secret <return>
         *
         * @return - a boolean value that indicates if the decoding succeeds
         */
        bool caontRSDecoding(unsigned char * shareBuffer, int *kShareIDList, int shareSize, int secretSize, unsigned char *secretBuffer);

    public:
        /*
         * constructor of CDCodec
         *
         * @param CDType - convergent dispersal type
         * @param n - total number of shares generated from a secret
         * @param m - reliability degree (i.e. maximum number of lost shares that can be tolerated)
         * @param r - confidentiality degree (i.e. maximum number of shares from which nothing can be derived)
         * @param cryptoObj - the CryptoPrimitive instance for hash generation and data encryption
         */
        CDCodec(int CDType = CAONT_RS_TYPE, 
                int n = 4, 
                int m =1, 
                int r = 2, 
                CryptoPrimitive *cryptoObj = new CryptoPrimitive(HIGH_SEC_PAIR_TYPE));

        /* 
         * destructor of CDCodec 
         */
        ~CDCodec();

        /*
         * encode a secret into n shares
         *
         * @param secretBuffer - a buffer that stores the secret
         * @param secretSize - the size of the secret
         * @param shareBuffer - a buffer for storing the n generated shares <return>
         * @param shareSize - the size of each share <return>
         *
         * @return - a boolean value that indicates if the encoding succeeds
         */
        bool encoding(unsigned char *secretBuffer, int secretSize, unsigned char *shareBuffer, int *shareSize);	

        /*
         * decode the secret from k = n - m shares
         *
         * @param shareBuffer - a buffer that stores the k shares 
         * @param kShareIDList - a list that stores the IDs of the k shares
         * @param shareSize - the size of each share 
         * @param secretSize - the size of the secret
         * @param secretBuffer - a buffer for storing the secret <return>
         *
         * @return - a boolean value that indicates if the decoding succeeds
         */
        bool decoding(unsigned char * shareBuffer, int *kShareIDList, int shareSize, int secretSize, unsigned char *secretBuffer);
};

#endif
