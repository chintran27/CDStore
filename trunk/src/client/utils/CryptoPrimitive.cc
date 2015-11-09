/*
 * CryptoPrimitive.cc
 */

#include "CryptoPrimitive.hh"

using namespace std;

/*initialize the static variable*/
opensslLock_t *CryptoPrimitive::opensslLock_ = NULL;

/*
 * OpenSSL locking callback function
 */
void CryptoPrimitive::opensslLockingCallback_(int mode, int type, const char *file, int line) {
#if OPENSSL_DEBUG
	CRYPTO_THREADID id;
	CRYPTO_THREADID_current(&id);
	/*'file' and 'line' are the file number of the function setting the lock. They can be useful for debugging.*/
	fprintf(stderr,"thread=%4ld, mode=%s, lock=%s, %s:%d\n", id.val, (mode&CRYPTO_LOCK)?"l":"u", 
			(type&CRYPTO_READ)?"r":"w", file, line);
#endif

	if (mode & CRYPTO_LOCK) {
		pthread_mutex_lock(&(opensslLock_->lockList[type]));
		CryptoPrimitive::opensslLock_->cntList[type]++;
	}
	else {
		pthread_mutex_unlock(&(opensslLock_->lockList[type]));
	}
}

/*
 * get the id of the current thread
 *
 * @param id - the thread id <return>
 */
void CryptoPrimitive::opensslThreadID_(CRYPTO_THREADID *id) {
	CRYPTO_THREADID_set_numeric(id, pthread_self());
}

/*
 * set up OpenSSL locks
 *
 * @return - a boolean value that indicates if the setup succeeds
 */
bool CryptoPrimitive::opensslLockSetup() {
#if defined(OPENSSL_THREADS)
	fprintf(stderr,"OpenSSL lock setup started\n");

	opensslLock_ = (opensslLock_t *) malloc(sizeof(opensslLock_t));

	opensslLock_->lockList = (pthread_mutex_t *) OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
	opensslLock_->cntList = (long *) OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));

	fprintf(stderr,"cntList[i]:CRYPTO_get_lock_name(i)\n");
	for (int i = 0; i < CRYPTO_num_locks(); i++) {
		pthread_mutex_init(&(opensslLock_->lockList[i]), NULL);
		opensslLock_->cntList[i] = 0;
		fprintf(stderr,"%8ld:%s\n", opensslLock_->cntList[i], CRYPTO_get_lock_name(i));
	}

	CRYPTO_THREADID_set_callback(&opensslThreadID_);
	CRYPTO_set_locking_callback(&opensslLockingCallback_);

	fprintf(stderr,"OpenSSL lock setup done\n");

	return 1;
#else
	fprintf(stderr, "Error: OpenSSL was not configured with thread support!\n");	

	return 0;
#endif
}

/*
 * clean up OpenSSL locks
 *
 * @return - a boolean value that indicates if the cleanup succeeds
 */
bool CryptoPrimitive::opensslLockCleanup() {
#if defined(OPENSSL_THREADS)
	CRYPTO_set_locking_callback(NULL);

	fprintf(stderr,"OpenSSL lock cleanup started\n");

	fprintf(stderr,"cntList[i]:CRYPTO_get_lock_name(i)\n");
	for (int i = 0; i < CRYPTO_num_locks(); i++) {
		pthread_mutex_destroy(&(opensslLock_->lockList[i]));
		fprintf(stderr,"%8ld:%s\n", opensslLock_->cntList[i], CRYPTO_get_lock_name(i));
	}

	OPENSSL_free(opensslLock_->lockList);
	OPENSSL_free(opensslLock_->cntList);
	free(opensslLock_);

	fprintf(stderr,"OpenSSL lock cleanup done\n");

	return 1;
#else
	fprintf(stderr, "Error: OpenSSL was not configured with thread support!\n");	

	return 0;
#endif
}

/*
 * constructor of CryptoPrimitive
 *
 * @param cryptoType - the type of CryptoPrimitive
 */
CryptoPrimitive::CryptoPrimitive(int cryptoType){
	cryptoType_ = cryptoType;

#if defined(OPENSSL_THREADS)
	/*check if opensslLockSetup() has been called to set up OpenSSL locks*/
	if (opensslLock_ == NULL) {	
		fprintf(stderr, "Error: opensslLockSetup() was not called before initializing CryptoPrimitive instances\n");				
		exit(1);		
	}

	if (cryptoType_ == HIGH_SEC_PAIR_TYPE) {
		/*allocate, initialize and return the digest context mdctx_*/
		EVP_MD_CTX_init(&mdctx_);

		/*get the EVP_MD structure for SHA-256*/
		md_ = EVP_sha256();
		hashSize_ = 32;

		/*initializes cipher contex cipherctx_*/
		EVP_CIPHER_CTX_init(&cipherctx_);

		/*get the EVP_CIPHER structure for AES-256*/
		cipher_ = EVP_aes_256_cbc();
		keySize_ = 32;
		blockSize_ = 16;

		/*allocate a constant IV*/
		iv_ = (unsigned char *) malloc(sizeof(unsigned char) * blockSize_);
		memset(iv_, 0, blockSize_); 	

		fprintf(stderr, "\nA CryptoPrimitive based on a pair of SHA-256 and AES-256 has been constructed! \n");		
		fprintf(stderr, "Parameters: \n");			
		fprintf(stderr, "      hashSize_: %d \n", hashSize_);				
		fprintf(stderr, "      keySize_: %d \n", keySize_);			
		fprintf(stderr, "      blockSize_: %d \n", blockSize_);		
		fprintf(stderr, "\n");
	}

	if (cryptoType_ == LOW_SEC_PAIR_TYPE) {
		/*allocate, initialize and return the digest context mdctx_*/
		EVP_MD_CTX_init(&mdctx_);

		/*get the EVP_MD structure for MD5*/
		md_ = EVP_md5();
		hashSize_ = 16;

		/*initializes cipher contex cipherctx_*/
		EVP_CIPHER_CTX_init(&cipherctx_);

		/*get the EVP_CIPHER structure for AES-128*/
		cipher_ = EVP_aes_128_cbc();
		keySize_ = 16;
		blockSize_ = 16;

		/*allocate a constant IV*/
		iv_ = (unsigned char *) malloc(sizeof(unsigned char) * blockSize_);
		memset(iv_, 0, blockSize_); 	

		fprintf(stderr, "\nA CryptoPrimitive based on a pair of MD5 and AES-128 has been constructed! \n");		
		fprintf(stderr, "Parameters: \n");			
		fprintf(stderr, "      hashSize_: %d \n", hashSize_);				
		fprintf(stderr, "      keySize_: %d \n", keySize_);			
		fprintf(stderr, "      blockSize_: %d \n", blockSize_);		
		fprintf(stderr, "\n");
	}

	if (cryptoType_ == SHA256_TYPE) {
		/*allocate, initialize and return the digest context mdctx_*/
		EVP_MD_CTX_init(&mdctx_);

		/*get the EVP_MD structure for SHA-256*/
		md_ = EVP_sha256();
		hashSize_ = 32;

		keySize_ = -1;
		blockSize_ = -1;

		fprintf(stderr, "\nA CryptoPrimitive based on SHA-256 has been constructed! \n");		
		fprintf(stderr, "Parameters: \n");			
		fprintf(stderr, "      hashSize_: %d \n", hashSize_);	
		fprintf(stderr, "\n");
	}

	if (cryptoType_ == SHA1_TYPE) {
		/*allocate, initialize and return the digest context mdctx_*/
		EVP_MD_CTX_init(&mdctx_);

		/*get the EVP_MD structure for SHA-1*/
		md_ = EVP_sha1();
		hashSize_ = 20;

		keySize_ = -1;
		blockSize_ = -1;

		fprintf(stderr, "\nA CryptoPrimitive based on SHA-1 has been constructed! \n");		
		fprintf(stderr, "Parameters: \n");			
		fprintf(stderr, "      hashSize_: %d \n", hashSize_);		
		fprintf(stderr, "\n");
	}	

#else
	fprintf(stderr, "Error: OpenSSL was not configured with thread support!\n");				
	exit(1);
#endif
}
/*
 * destructor of CryptoPrimitive
 */
CryptoPrimitive::~CryptoPrimitive(){
	if ((cryptoType_ == HIGH_SEC_PAIR_TYPE) || (cryptoType_ == LOW_SEC_PAIR_TYPE)) {
		/*clean up the digest context mdctx_ and free up the space allocated to it*/
		EVP_MD_CTX_cleanup(&mdctx_);

		/*clean up the cipher context cipherctx_ and free up the space allocated to it*/
		EVP_CIPHER_CTX_cleanup(&cipherctx_);
		free(iv_);	
	}

	if ((cryptoType_ == SHA256_TYPE) || (cryptoType_ == SHA1_TYPE)) {
		/*clean up the digest context mdctx_ and free up the space allocated to it*/
		EVP_MD_CTX_cleanup(&mdctx_);
	}

	fprintf(stderr, "\nThe CryptoPrimitive has been destructed! \n");
	fprintf(stderr, "\n");
}

/*
 * get the hash size
 *
 * @return - the hash size
 */
int CryptoPrimitive::getHashSize() {
	return hashSize_;
}

/*
 * get the key size
 *
 * @return - the key size
 */
int CryptoPrimitive::getKeySize() {
	return keySize_;
}

/*
 * get the size of the encryption block unit
 *
 * @return - the block size
 */
int CryptoPrimitive::getBlockSize() {
	return blockSize_;
}

/*
 * generate the hash for the data stored in a buffer
 *
 * @param dataBuffer - the buffer that stores the data
 * @param dataSize - the size of the data
 * @param hash - the generated hash <return>
 *
 * @return - a boolean value that indicates if the hash generation succeeds
 */
bool CryptoPrimitive::generateHash(unsigned char *dataBuffer, const int &dataSize, unsigned char *hash) {
	int hashSize;

	EVP_DigestInit_ex(&mdctx_, md_, NULL);
	EVP_DigestUpdate(&mdctx_, dataBuffer, dataSize);
	EVP_DigestFinal_ex(&mdctx_, hash, (unsigned int*) &hashSize);

	if (hashSize != hashSize_) {
		fprintf(stderr, "Error: the size of the generated hash (%d bytes) does not match with the expected one (%d bytes)!\n", 
				hashSize, hashSize_);

		return 0;
	}	

	return 1;
}


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
bool CryptoPrimitive::encryptWithKey(unsigned char *dataBuffer, const int &dataSize, unsigned char *key, 
		unsigned char *ciphertext) {
	int ciphertextSize, ciphertextTailSize;	

	if (dataSize % blockSize_ != 0) {
		fprintf(stderr, "Error: the size of the input data (%d bytes) is not a multiple of that of encryption block unit (%d bytes)!\n", 
				dataSize, blockSize_);

		return 0;
	}

	EVP_EncryptInit_ex(&cipherctx_, cipher_, NULL, key, iv_);		
	/*disable padding to ensure that the generated ciphertext has the same size as the input data*/
	EVP_CIPHER_CTX_set_padding(&cipherctx_, 0);
	EVP_EncryptUpdate(&cipherctx_, ciphertext, &ciphertextSize, dataBuffer, dataSize);
	EVP_EncryptFinal_ex(&cipherctx_, ciphertext + ciphertextSize, &ciphertextTailSize);
	ciphertextSize += ciphertextTailSize;

	if (ciphertextSize != dataSize) {
		fprintf(stderr, "Error: the size of the cipher output (%d bytes) does not match with that of the input (%d bytes)!\n", 
				ciphertextSize, dataSize);

		return 0;
	}	

	return 1;
}
