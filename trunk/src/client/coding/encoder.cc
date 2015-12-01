/*
 * encoder.cc
 *
 */

#include "encoder.hh"

using namespace std;

/*
 * thread handler for encoding each secret into shares
 *
 * @param param - parameters for encode thread
 */
void* Encoder::thread_handler(void* param){

    /* parse parameters */
    int index = ((param_encoder*)param)->index;
    Encoder* obj = ((param_encoder*)param)->obj;
    free(param);

    /* main loop for getting secrets and encode them into shares*/
    while(true){

        /* get an object from input buffer */
        Secret_Item_t temp;
        ShareChunk_Item_t input;
        obj->inputbuffer_[index]->Extract(&temp);

        /* get the object type */
        int type = temp.type;
        input.type = type;

        /* copy content into input object */
        if(type == FILE_OBJECT){
            /* if it's file header */
            memcpy(&input.file_header, &temp.file_header, sizeof(fileHead_t));
        }else{

            /* if it's share object */
            obj->encodeObj_[index]->encoding(temp.secret.data, temp.secret.secretSize, input.share_chunk.data, &(input.share_chunk.shareSize));
            input.share_chunk.secretID = temp.secret.secretID;
            input.share_chunk.secretSize = temp.secret.secretSize;
            input.share_chunk.end = temp.secret.end;
        }

        /* add the object to output buffer */
        obj->outputbuffer_[index]->Insert(&input,sizeof(input));
    }
    return NULL;
}

/*
 * collect thread for getting share object in order
 *
 * @param param - parameters for collect thread
 */
void* Encoder::collect(void* param){
    /* index for sequencially collect shares */
    int nextBufferIndex = 0;

    /* parse parameters */
    Encoder* obj = (Encoder*)param;

    /* main loop for collecting shares */
    while(true){

        /* extract an object from a certain ringbuffer */
        ShareChunk_Item_t temp;
        obj->outputbuffer_[nextBufferIndex]->Extract(&temp);
        nextBufferIndex = (nextBufferIndex + 1)%NUM_THREADS;

        /* get the object type */
        int type = temp.type;

        Uploader::Item_t input;
        if(type == FILE_OBJECT){

            /* if it's file header, directly transform the object to uploader */
            input.type = FILE_HEADER;

            /* copy file header information */
            input.fileObj.file_header.fileSize = temp.file_header.fileSize;
            input.fileObj.file_header.numOfPastSecrets = 0;
            input.fileObj.file_header.sizeOfPastSecrets = 0;
            input.fileObj.file_header.numOfComingSecrets = 0;
            input.fileObj.file_header.sizeOfComingSecrets = 0;
            
            unsigned char tmp[temp.file_header.fullNameSize*32];
            int tmp_s;

            //encode pathname into shares for privacy
            obj->encodeObj_[0]->encoding(temp.file_header.data, temp.file_header.fullNameSize, tmp, &(tmp_s));
            
            input.fileObj.file_header.fullNameSize = tmp_s;

            /* copy file name */
            //memcpy(input.fileObj.data, temp.file_header.data, temp.file_header.fullNameSize);

#ifndef ENCODE_ONLY_MODE
            /* add the object to each cloud's uploader buffer */
            for(int i = 0; i < obj->n_; i++){

                //copy the corresponding share as file name
                memcpy(input.fileObj.data, tmp+i*tmp_s, input.fileObj.file_header.fullNameSize);
                obj->uploadObj_->add(&input, sizeof(input), i);
            }
#endif
        }else{

            /* if it's share object */
            for(int i = 0; i < obj->n_; i++){
                input.type = SHARE_OBJECT;

                /* copy share info */	
                int shareSize = temp.share_chunk.shareSize;
                input.shareObj.share_header.secretID = temp.share_chunk.secretID;
                input.shareObj.share_header.secretSize = temp.share_chunk.secretSize;
                input.shareObj.share_header.shareSize = shareSize;
                memcpy(input.shareObj.data, temp.share_chunk.data+(i*shareSize), shareSize);
#ifndef ENCODE_ONLY_MODE
#endif
                /* see if it's the last secret of a file */
                if (temp.share_chunk.end == 1) input.type = SHARE_END;
#ifdef ENCODE_ONLY_MODE
                if (temp.share_chunk.end == 1) pthread_exit(NULL);
#else 
                /* add the share object to targeting cloud uploader buffer */
                obj->uploadObj_->add(&input, sizeof(input), i);
#endif
            }
        }
    }
    return NULL;
}


/*
 * see if it's end of encoding file
 *
 */
void Encoder::indicateEnd(){
    pthread_join(tid_[NUM_THREADS],NULL);
}

/*
 * constructor
 *    
 * @param type - convergent dispersal type
 * @param n - total number of shares generated from a secret
 * @param m - reliability degree
 * @param r - confidentiality degree
 * @param securetype - encryption and hash type
 * @param uploaderObj - pointer link to uploader object
 *
 */
Encoder::Encoder(int type, int n, int m, int r, int securetype, Uploader* uploaderObj){

    /* initialization of variables */
    int i;
    n_ = n;
    nextAddIndex_ = 0;
    cryptoObj_ = (CryptoPrimitive**)malloc(sizeof(CryptoPrimitive*)*NUM_THREADS);
    inputbuffer_ = (RingBuffer<Secret_Item_t>**)malloc(sizeof(RingBuffer<Secret_Item_t>*)*NUM_THREADS);
    outputbuffer_ = (RingBuffer<ShareChunk_Item_t>**)malloc(sizeof(RingBuffer<ShareChunk_Item_t>*)*NUM_THREADS);

    /* initialization of objects */
    for (i = 0; i < NUM_THREADS; i++){
        inputbuffer_[i] = new RingBuffer<Secret_Item_t>(RB_SIZE, true, 1);
        outputbuffer_[i] = new RingBuffer<ShareChunk_Item_t>(RB_SIZE, true, 1);
        cryptoObj_[i] = new CryptoPrimitive(securetype);
        encodeObj_[i] = new CDCodec(type,n,m,r, cryptoObj_[i]);
        param_encoder* temp = (param_encoder*)malloc(sizeof(param_encoder));
        temp->index = i;
        temp->obj = this;

        /* create encoding threads */
        pthread_create(&tid_[i],0,&thread_handler,(void*)temp);
    }

    uploadObj_ = uploaderObj;

    /* create collect thread */
    pthread_create(&tid_[NUM_THREADS],0,&collect,(void*)this);
}

/*
 * destructor
 *
 */
Encoder::~Encoder(){
    for (int i = 0; i < NUM_THREADS; i++){
        delete(cryptoObj_[i]);
        delete(encodeObj_[i]);
        delete(inputbuffer_[i]);
        delete(outputbuffer_[i]);
    }
    free(inputbuffer_);
    free(outputbuffer_);
    free(cryptoObj_);
}

/*
 * add function for sequencially add items to each encode buffer
 *
 * @param item - input object
 *
 */
int Encoder::add(Secret_Item_t* item){
    /* add item */
    inputbuffer_[nextAddIndex_]->Insert(item, sizeof(Secret_Item_t));

    /* increment the index */
    nextAddIndex_ = (nextAddIndex_+1)%NUM_THREADS;
    return 1;
}


