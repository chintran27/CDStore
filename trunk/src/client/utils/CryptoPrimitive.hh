/*
 * CryptoPrimitive.hh
 */

#ifndef __CRYPTOPRIMITIVE_HH__
#define __CRYPTOPRIMITIVE_HH__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /*for uint32_t*/
#include <string.h>

/*for the use of OpenSSL*/
#include <openssl/evp.h>
#include <openssl/crypto.h>
#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
/*macro for OpenSSL debug*/
#define OPENSSL_DEBUG 1
/*for the use of mutex lock*/
#include <pthread.h>

/*macro for the type of a high secure pair of hash generation and encryption*/
#define HIGH_SEC_PAIR_TYPE 0
/*macro for the type of a low secure pair of hash generation and encryption*/
#define LOW_SEC_PAIR_TYPE 1
/*macro for the type of a SHA-256 hash generation*/
#define SHA256_TYPE 2
/*macro for the type of a SHA-1 hash generation*/
#define SHA1_TYPE 3

using namespace std;

typedef struct {
	pthread_mutex_t *lockList;
	long *cntList;
}opensslLock_t;

class CryptoPrimitive{
	private:
		/*the type of CryptoPrimitive*/
		int cryptoType_;

		/*variables used in hash calculation*/
		EVP_MD_CTX mdctx_;
		const EVP_MD *md_;
		/*the size of the generated hash*/
		int hashSize_;

		/*variables used in encryption*/
		EVP_CIPHER_CTX cipherctx_;
		const EVP_CIPHER *cipher_;
		unsigned char *iv_;

		/*the size of the key for encryption*/
		int keySize_;
		/*the size of the encryption block unit*/
		int blockSize_;

		/*OpenSSL lock*/
		static opensslLock_t *opensslLock_;

		/*
		 * OpenSSL locking callback function
		 */
		static void opensslLockingCallback_(int mode, int type, const char *file, int line);

		/*
		 * get the id of the current thread
		 *
		 * @param id - the thread id <return>
		 */
		static void opensslThreadID_(CRYPTO_THREADID *id);

	public:
		/*
		 * constructor of CryptoPrimitive
		 *
		 * @param cryptoType - the type of CryptoPrimitive
		 */
		CryptoPrimitive(int cryptoType = HIGH_SEC_PAIR_TYPE);

		/*
		 * destructor of CryptoPrimitive
		 */
		~CryptoPrimitive();

		/*
		 * set up OpenSSL locks
		 *
		 * @return - a boolean value that indicates if the setup succeeds
		 */
		static bool opensslLockSetup();

		/*
		 * clean up OpenSSL locks
		 *
		 * @return - a boolean value that indicates if the cleanup succeeds
		 */
		static bool opensslLockCleanup();

		/*
		 * get the hash size
		 *
		 * @return - the hash size
		 */
		int getHashSize();

		/*
		 * get the key size
		 *
		 * @return - the key size
		 */
		int getKeySize();

		/*
		 * get the size of the encryption block unit
		 *
		 * @return - the block size
		 */
		int getBlockSize();

		/*
		 * generate the hash for the data stored in a buffer
		 *
		 * @param dataBuffer - the buffer that stores the data
		 * @param dataSize - the size of the data
		 * @param hash - the generated hash <return>
		 *
		 * @return - a boolean value that indicates if the hash generation succeeds
		 */
		bool generateHash(unsigned char *dataBuffer, const int &dataSize, unsigned char *hash);

		/*
		 * encrypt the data stored in a buffer with a key
		 *
		 * @param dataBuffer - the buffer that stores the data
		 * @param dataSize - the size of the data
		 * @param key - the key used to encrypt the data
		 * @param ciphertext - the generated ciphertext <return>
		 *
		 * @return - a boolean value that indicates if the encryption succeeds
		 */
		bool encryptWithKey(unsigned char *dataBuffer, const int &dataSize, unsigned char *key, 
				unsigned char *ciphertext);
};

#endif
