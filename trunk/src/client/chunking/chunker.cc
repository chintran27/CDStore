/*
 * Chunker.cc
 */

#include "chunker.hh"

using namespace std;

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
Chunker::Chunker(bool chunkerType, int avgChunkSize, int minChunkSize, int maxChunkSize, int slidingWinSize){
    chunkerType_ = chunkerType;

    if (chunkerType_ == FIX_SIZE_TYPE) { /*fixed-size chunker*/
        avgChunkSize_ = avgChunkSize;

        fprintf(stderr, "\nA fixed-size chunker has been constructed! \n");
        fprintf(stderr, "Parameters: \n");
        fprintf(stderr, "      avgChunkSize_: %d \n", avgChunkSize_);	
        fprintf(stderr, "\n");
    }

    if (chunkerType_ == VAR_SIZE_TYPE) { /*variable-size chunker*/
        int numOfMaskBits, i;

        if (minChunkSize >= avgChunkSize)  {
            fprintf(stderr, "Error: minChunkSize should be smaller than avgChunkSize!\n");	
            exit(1);
        }
        if (maxChunkSize <= avgChunkSize)  {
            fprintf(stderr, "Error: maxChunkSize should be larger than avgChunkSize!\n");
            exit(1);
        }
        avgChunkSize_ = avgChunkSize;
        minChunkSize_ = minChunkSize;	
        maxChunkSize_ = maxChunkSize;

        slidingWinSize_ = slidingWinSize;

        /*initialize the base and modulus for calculating the fingerprint of a window*/
        /*these two values were employed in open-vcdiff: "http://code.google.com/p/open-vcdiff/"*/
        polyBase_ = 257; /*a prime larger than 255, the max value of "unsigned char"*/
        polyMOD_ = (1 << 23); /*polyMOD_ - 1 = 0x7fffff: use the last 23 bits of a polynomial as its hash*/

        /*initialize the lookup table for accelerating the power calculation in rolling hash*/
        powerLUT_ = (uint32_t *) malloc(sizeof(uint32_t) * slidingWinSize_);
        /*powerLUT_[i] = power(polyBase_, i) mod polyMOD_*/
        powerLUT_[0] = 1;
        for (i = 1; i < slidingWinSize_; i++) {
            /*powerLUT_[i] = (powerLUT_[i-1] * polyBase_) mod polyMOD_*/
            powerLUT_[i] = (powerLUT_[i-1] * polyBase_) & (polyMOD_ - 1); 
        }

        /*initialize the lookup table for accelerating the byte remove in rolling hash*/
        removeLUT_ = (uint32_t *) malloc(sizeof(uint32_t) * 256); /*256 for unsigned char*/
        for (i = 0; i < 256; i++) {
            /*removeLUT_[i] = (- i * powerLUT_[slidingWinSize_-1]) mod polyMOD_*/
            removeLUT_[i] = (i * powerLUT_[slidingWinSize_-1]) & (polyMOD_ - 1); 
            if (removeLUT_[i] != 0) removeLUT_[i] = polyMOD_ - removeLUT_[i];
            /*note: % is a remainder (rather than modulus) operator*/
            /*      if a < 0,  -polyMOD_ < a % polyMOD_ <= 0       */
        }

        /*initialize the mask for depolytermining an anchor*/
        /*note: power(2, numOfMaskBits) = avgChunkSize_*/
        numOfMaskBits = 1;		
        while ((avgChunkSize_ >> numOfMaskBits) != 1) numOfMaskBits++;
        anchorMask_ = (1 << numOfMaskBits) - 1;

        /*initialize the value for depolytermining an anchor*/
        anchorValue_ = 0;		

        fprintf(stderr, "\nA variable-size chunker has been constructed! \n");
        fprintf(stderr, "Parameters: \n");	
        fprintf(stderr, "      avgChunkSize_: %d \n", avgChunkSize_);		
        fprintf(stderr, "      minChunkSize_: %d \n", minChunkSize_);	
        fprintf(stderr, "      maxChunkSize_: %d \n", maxChunkSize_);
        fprintf(stderr, "      slidingWinSize_: %d \n", slidingWinSize_);		
        fprintf(stderr, "      polyBase_: 0x%x \n", polyBase_);	
        fprintf(stderr, "      polyMOD_: 0x%x \n", polyMOD_);
        fprintf(stderr, "      anchorMask_: 0x%x \n", anchorMask_);
        fprintf(stderr, "      anchorValue_: 0x%x \n", anchorValue_);	
        fprintf(stderr, "\n");
    }	
}

/*
 * destructor of Chunker
 */
Chunker::~Chunker(){
    if (chunkerType_ == VAR_SIZE_TYPE) { /*variable-size chunker*/
        free(powerLUT_);
        free(removeLUT_);

        fprintf(stderr, "\nThe fixed-size chunker has been destructed! \n");	
        fprintf(stderr, "\n");
    }

    if (chunkerType_ == VAR_SIZE_TYPE) { /*variable-size chunker*/
        fprintf(stderr, "\nThe variable-size chunker has been destructed! \n");	
        fprintf(stderr, "\n");
    }
}

/*
 * divide a buffer into a number of fixed-size chunks
 *
 * @param buffer - a buffer to be chunked
 * @param bufferSize - the size of the buffer
 * @param chunkEndIndexList - a list for returning the end index of each chunk <return>
 * @param numOfChunks - the number of chunks <return>
 */
void Chunker::fixSizeChunking(unsigned char *buffer, int bufferSize, int *chunkEndIndexList, int *numOfChunks){
    int chunkEndIndex;

    (*numOfChunks) = 0;
    chunkEndIndex = -1 + avgChunkSize_;

    /*divide the buffer into chunks*/
    while (chunkEndIndex < bufferSize) {
        /*record the end index of a chunk*/
        chunkEndIndexList[(*numOfChunks)] = chunkEndIndex;

        /*go on for the next chunk*/
        chunkEndIndex = chunkEndIndexList[(*numOfChunks)] + avgChunkSize_;		
        (*numOfChunks)++;
    }	

    /*deal with the tail of the buffer*/
    if (((*numOfChunks) == 0) || (((*numOfChunks) > 0) && (chunkEndIndexList[(*numOfChunks)-1] != bufferSize -1))) { 
        /*note: such a tail chunk has a size < avgChunkSize_*/
        chunkEndIndexList[(*numOfChunks)] = bufferSize -1;		
        (*numOfChunks)++;
    }	
}

/*
 * divide a buffer into a number of variable-size chunks
 *
 * @param buffer - a buffer to be chunked
 * @param bufferSize - the size of the buffer
 * @param chunkEndIndexList - a list for returning the end index of each chunk <return>
 * @param numOfChunks - the number of chunks <return>
 */
void Chunker::varSizeChunking(unsigned char *buffer, int bufferSize, int *chunkEndIndexList, int *numOfChunks){
    int chunkEndIndex, chunkEndIndexLimit;
    uint32_t winFp; /*the fingerprint of a window*/
    int i;

    /*note: to improve performance, we use the optimization in open-vcdiff: "http://code.google.com/p/open-vcdiff/"*/

    (*numOfChunks) = 0;
    chunkEndIndex = -1 + minChunkSize_;
    chunkEndIndexLimit = -1 + maxChunkSize_;

    /*divide the buffer into chunks*/
    while (chunkEndIndex < bufferSize) {	
        if (chunkEndIndexLimit >= bufferSize) chunkEndIndexLimit = bufferSize - 1;		

        /*calculate the fingerprint of the first window*/
        winFp = 0;
        for (i = 0; i < slidingWinSize_; i++) {
            /*winFp = winFp + ((buffer[chunkEndIndex-i] * powerLUT_[i]) mod polyMOD_)*/
            winFp = winFp + ((buffer[chunkEndIndex-i] * powerLUT_[i]) & (polyMOD_ - 1));
        }
        /*winFp = winFp mod polyMOD_*/
        winFp = winFp & (polyMOD_ - 1);

        while (((winFp & anchorMask_) != anchorValue_) && (chunkEndIndex < chunkEndIndexLimit)) {
            /*move the window forward by 1 byte*/
            chunkEndIndex++;

            /*update the fingerprint based on rolling hash*/
            /*winFp = ((winFp + removeLUT_[buffer[chunkEndIndex-slidingWinSize_]]) * polyBase_ + buffer[chunkEndIndex]) mod polyMOD_*/
            winFp = ((winFp + removeLUT_[buffer[chunkEndIndex-slidingWinSize_]]) * polyBase_ + buffer[chunkEndIndex]) & (polyMOD_ - 1); 
        }

        /*record the end index of a chunk*/
        chunkEndIndexList[(*numOfChunks)] = chunkEndIndex;

        /*go on for the next chunk*/
        chunkEndIndex = chunkEndIndexList[(*numOfChunks)] + minChunkSize_;
        chunkEndIndexLimit = chunkEndIndexList[(*numOfChunks)] + maxChunkSize_;
        (*numOfChunks)++;
    }

    /*deal with the tail of the buffer*/
    if (((*numOfChunks) == 0) || (((*numOfChunks) > 0) && (chunkEndIndexList[(*numOfChunks)-1] != bufferSize -1))) { 
        /*note: such a tail chunk has a size < minChunkSize_*/
        chunkEndIndexList[(*numOfChunks)] = bufferSize -1;		
        (*numOfChunks)++;
    }
}

/*
 * divide a buffer into a number of chunks
 *
 * @param buffer - a buffer to be chunked
 * @param bufferSize - the size of the buffer
 * @param chunkEndIndexList - a list for returning the end index of each chunk <return>
 * @param numOfChunks - the number of chunks <return>
 */
void Chunker::chunking(unsigned char *buffer, int bufferSize, int *chunkEndIndexList, int *numOfChunks){
    if (chunkerType_ == FIX_SIZE_TYPE) { /*fixed-size chunker*/
        fixSizeChunking(buffer, bufferSize, chunkEndIndexList, numOfChunks);
    }

    if (chunkerType_ == VAR_SIZE_TYPE) { /*variable-size chunker*/
        varSizeChunking(buffer, bufferSize, chunkEndIndexList, numOfChunks);
    }	
}

