/*
 * conf.hh
 */

#ifndef __CONF_HH__
#define __CONF_HH__

#include <stdio.h>
#include <stdlib.h>

using namespace std;

/*
 * configuration class
 */

class Configuration{
  private:
      /* total number for cloud */
      int n_;

      /* fault tolerance degree */
      int m_;

      /* k = n - m */
      int k_;

      /* security degree */
      int r_;

      /* secret buffer size */
      int secretBufferSize_;

      /* share buffer size */
      int shareBufferSize_;

      /* buffer size */
      int bufferSize_;

      /* chunk end list size */
      int chunkEndIndexListSize_;
  public:
      /* constructor */
      Configuration(){
        n_ = 4;
        m_ = 1;
        k_ = n_ - m_;
        r_ = k_ - 1;
        secretBufferSize_ = 16*1024;
        shareBufferSize_ = 16*1024*n_;
        bufferSize_ = 128*1024*1024;
        chunkEndIndexListSize_ = 1024*1024;
      }

      inline int getN() { return n_; }

      inline int getM() { return m_; }

      inline int getK() { return k_; }

      inline int getR() { return r_; }

      inline int getSecretBufferSize() { return secretBufferSize_; }

      inline int getShareBufferSize() { return shareBufferSize_; }

      inline int getBufferSize() { return bufferSize_; }

      inline int getListSize() { return chunkEndIndexListSize_; }

};

#endif
