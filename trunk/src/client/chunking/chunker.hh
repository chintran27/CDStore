/*
 * Chunker.hh
 */

#ifndef __CHUNKER_HH__
#define __CHUNKER_HH__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /*for uint32_t*/

/*macro for the type of fixed-size chunker*/
#define FIX_SIZE_TYPE 0
/*macro for the type of variable-size chunker*/
#define VAR_SIZE_TYPE 1

using namespace std;

class Chunker{
    private:
        /*chunker type (FIX_SIZE_TYPE or VAR_SIZE_TYPE)*/
        bool chunkerType_; 

        /*average chunk size*/
        int avgChunkSize_; 
        /*minimum chunk size*/
        int minChunkSize_; 
        /*maximum chunk size*/
        int maxChunkSize_; 

        /*sliding window size*/
        int slidingWinSize_; 

        /*the base for calculating the value of the polynomial in rolling hash*/
        uint32_t polyBase_; 
        /*the modulus for limiting the value of the polynomial in rolling hash*/
        uint32_t polyMOD_; 		
        /*note: to avoid overflow, polyMOD_*255 should be in the range of "uint32_t"*/
        /*      here, 255 is the max value of "unsigned char"                       */

        /*the lookup table for accelerating the power calculation in rolling hash*/
        uint32_t *powerLUT_; 
        /*the lookup table for accelerating the byte remove in rolling hash*/
        uint32_t *removeLUT_; 

        /*the mask for determining an anchor*/
        uint32_t anchorMask_;	
        /*the value for determining an anchor*/
        uint32_t anchorValue_; 

        /*
         * divide a buffer into a number of fixed-size chunks
         *
         * @param buffer - a buffer to be chunked
         * @param bufferSize - the size of the buffer
         * @param chunkEndIndexList - a list for returning the end index of each chunk <return>
         * @param numOfChunks - the number of chunks <return>
         */
        void fixSizeChunking(unsigned char *buffer, int bufferSize, int *chunkEndIndexList, int *numOfChunks);

        /*
         * divide a buffer into a number of variable-size chunks
         *
         * @param buffer - a buffer to be chunked
         * @param bufferSize - the size of the buffer
         * @param chunkEndIndexList - a list for returning the end index of each chunk <return>
         * @param numOfChunks - the number of chunks <return>
         */
        void varSizeChunking(unsigned char *buffer, int bufferSize, int *chunkEndIndexList, int *numOfChunks);

    public:
        /*
         * constructor of Chunker
         *
         * @param chunkerType - chunker type (FIX_SIZE_TYPE or VAR_SIZE_TYPE)
         * @param avgChunkSize - average chunk size
         * @param minChunkSize - minimum chunk size
         * @param maxChunkSize - maximum chunk size
         * @param slidingWinSize - sliding window size
         *
         * NOTE: if chunkerType = FIX_SIZE_TYPE, only input avgChunkSize
         */
        Chunker(bool chunkerType = VAR_SIZE_TYPE, 
                int avgChunkSize = (8<<10), 
                int minChunkSize = (2<<10), 
                int maxChunkSize = (16<<10), 
                int slidingWinSize = 48);

        /*
         * destructor of Chunker
         */
        ~Chunker();

        /*
         * divide a buffer into a number of chunks
         *
         * @param buffer - a buffer to be chunked
         * @param bufferSize - the size of the buffer
         * @param chunkEndIndexList - a list for returning the end index of each chunk <return>
         * @param numOfChunks - the number of chunks <return>
         */
        void chunking(unsigned char *buffer, int bufferSize, int *chunkEndIndexList, int *numOfChunks);
};

#endif
