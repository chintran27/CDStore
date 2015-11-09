/*
 * BackendStorer.cc
 */

#include "BackendStorer.hh"

using namespace std;

/*
 * constructor of BackendStorer
 *
 * @param cacheDirName - the name of the directory that acts as a cache
 * @param maxCacheSize - the maximum cache size
 * @param backendIP - the IP of the server that keeps the backend storage
 * @param backendDirName - the name of the directory that acts as the backend storage
 */
BackendStorer::BackendStorer(const std::string &cacheDirName, long maxCacheSize, 
		const std::string &backendIP, const std::string &backendDirName) {
	cacheDirName_ = cacheDirName;
	/*format the input dir name cacheDirName*/
	if (!formatDirName_(cacheDirName_)) {
		fprintf(stderr, "Error: the input 'cacheDirName' is invalid!\n");
		exit(1);	
	}	

	/*maxCacheSize_ >= availCacheSize_ >= usedCacheSize_*/
	/*maxCacheSize_ - availCacheSize_ is the size of space occupied by 
	  new files that have not been stored into the backend storage*/
	if (maxCacheSize <= 0) {
		fprintf(stderr, "Error: the input 'maxCacheSize' is invalid!\n");
		exit(1);	
	}	
	maxCacheSize_ = maxCacheSize;
	availCacheSize_ = maxCacheSize;
	usedCacheSize_ = 0;

	backendType_ = IPSERVER;
	backendIP_ = backendIP;
	backendDirName_ = backendDirName;
	/*format the input IP backendIP*/
	if (!formatIP_(backendIP_)) {
		fprintf(stderr, "Error: the input 'backendIP' is invalid!\n");
		exit(1);	
	}	
	/*format the input dir name backendDirName*/
	if (!formatDirName_(backendDirName_)) {
		fprintf(stderr, "Error: the input 'backendDirName' is invalid!\n");
		exit(1);	
	}	

	headNewlyStoredFileNode_ = NULL;
	tailNewlyStoredFileNode_ = NULL;
	numOfNewlyStoredFiles_ = 0;
	/*initialize the mutex lock fileStorerLock_*/
	if (pthread_mutex_init(&fileStorerLock_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the mutex lock fileStorerLock_!\n");
		exit(1);	
	}
	/*initialize the condition fileStorerCond_*/
	if (pthread_cond_init(&fileStorerCond_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the condition fileStorerCond_!\n");
		exit(1);	
	}	

	headNewlyUsedFileNode_ = NULL;
	tailNewlyUsedFileNode_ = NULL;
	numOfNewlyUsedFiles_ = 0;
	/*initialize the mutex lock cacheUpdaterLock_*/
	if (pthread_mutex_init(&cacheUpdaterLock_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the mutex lock cacheUpdaterLock_!\n");
		exit(1);	
	}
	/*initialize the condition cacheUpdaterCond_*/
	if (pthread_cond_init(&cacheUpdaterCond_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the condition cacheUpdaterCond_!\n");
		exit(1);	
	}

	fileNodeSize_ = sizeof(fileNode_t);

	fprintf(stderr, "\nA BackendStorer has been constructed! \n");		
	fprintf(stderr, "Parameters: \n");		
	fprintf(stderr, "      cacheDirName_: %s \n", cacheDirName_.c_str());		
	fprintf(stderr, "      maxCacheSize_: %ld \n", maxCacheSize_);
	fprintf(stderr, "\n");	
}

/*
 * constructor of BackendStorer
 *
 * @param cacheDirName - the name of the directory that acts as a cache
 * @param maxCacheSize - the maximum cache size
 * @param backendType - the type of the backend storage (AMAZON, GOOGLE, MSAZURE, or RACKSPACE)
 */
BackendStorer::BackendStorer(const std::string &cacheDirName, long maxCacheSize, int backendType) {
	cacheDirName_ = cacheDirName;
	/*format the input dir name cacheDirName*/
	if (!formatDirName_(cacheDirName_)) {
		fprintf(stderr, "Error: the input 'cacheDirName' is invalid!\n");
		exit(1);	
	}	

	/*maxCacheSize_ >= availCacheSize_ >= usedCacheSize_*/
	/*maxCacheSize_ - availCacheSize_ is the size of space occupied by 
	  new files that have not been stored into the backend storage*/
	if (maxCacheSize <= 0) {
		fprintf(stderr, "Error: the input 'maxCacheSize' is invalid!\n");
		exit(1);	
	}	
	maxCacheSize_ = maxCacheSize;
	availCacheSize_ = maxCacheSize;
	usedCacheSize_ = 0;

	if ((backendType != AMAZON) && (backendType != GOOGLE) && 
			(backendType != MSAZURE) && (backendType != RACKSPACE)) {
		fprintf(stderr, "Error: the input 'backendType' is invalid!\n");
		exit(1);	
	}
	backendType_ = backendType;

	headNewlyStoredFileNode_ = NULL;
	tailNewlyStoredFileNode_ = NULL;
	numOfNewlyStoredFiles_ = 0;
	/*initialize the mutex lock fileStorerLock_*/
	if (pthread_mutex_init(&fileStorerLock_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the mutex lock fileStorerLock_!\n");
		exit(1);	
	}
	/*initialize the condition fileStorerCond_*/
	if (pthread_cond_init(&fileStorerCond_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the condition fileStorerCond_!\n");
		exit(1);	
	}	

	headNewlyUsedFileNode_ = NULL;
	tailNewlyUsedFileNode_ = NULL;
	numOfNewlyUsedFiles_ = 0;
	/*initialize the mutex lock cacheUpdaterLock_*/
	if (pthread_mutex_init(&cacheUpdaterLock_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the mutex lock cacheUpdaterLock_!\n");
		exit(1);	
	}
	/*initialize the condition cacheUpdaterCond_*/
	if (pthread_cond_init(&cacheUpdaterCond_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the condition cacheUpdaterCond_!\n");
		exit(1);	
	}

	fileNodeSize_ = sizeof(fileNode_t);

	fprintf(stderr, "\nA BackendStorer has been constructed! \n");		
	fprintf(stderr, "Parameters: \n");		
	fprintf(stderr, "      cacheDirName_: %s \n", cacheDirName_.c_str());		
	fprintf(stderr, "      maxCacheSize_: %ld \n", maxCacheSize_);
	fprintf(stderr, "\n");	
}

/* 
 * destructor of BackendStorer 
 */
BackendStorer::~BackendStorer() {
	fileNode_t *currBufferNode, *nextBufferNode;

	/*free all fileNode_t items*/
	if (headNewlyStoredFileNode_ != NULL) {
		currBufferNode = headNewlyStoredFileNode_;
		while (currBufferNode != NULL){
			nextBufferNode = currBufferNode->next;
			free(currBufferNode);
			currBufferNode = nextBufferNode;
		}	
	}	
	/*clean up the mutex lock fileStorerLock_*/	
	pthread_mutex_destroy(&fileStorerLock_);
	/*clean up the condition fileStorerCond_*/	
	pthread_cond_destroy(&fileStorerCond_);

	/*free all fileNode_t items*/
	if (headNewlyUsedFileNode_ != NULL) {
		currBufferNode = headNewlyUsedFileNode_;
		while (currBufferNode != NULL){
			nextBufferNode = currBufferNode->next;
			free(currBufferNode);
			currBufferNode = nextBufferNode;
		}	
	}
	/*clean up the mutex lock cacheUpdaterLock_*/	
	pthread_mutex_destroy(&cacheUpdaterLock_);
	/*clean up the condition cacheUpdaterCond_*/	
	pthread_cond_destroy(&cacheUpdaterCond_);	

	fprintf(stderr, "\nThe BackendStorer has been destructed! \n");	
	fprintf(stderr, "\n");
}

/*
 * format a directory name into '.../.../shortName/'
 *
 * @param dirName - the directory name to be formated <return>
 *
 * @return - a boolean value that indicates if the formating succeeds
 */
bool BackendStorer::formatDirName_(std::string &dirName) {
	/*check if it is empty*/
	if (dirName.empty()) {
		fprintf(stderr, "Error: the dir name is empty!\n");
		return 0;
	}

	/*finally, make sure that dirName ends with '/'*/
	if (dirName[dirName.size() - 1] != '/') {
		dirName += '/';
	}	

	return 1;
}

/*
 * format an IP address
 *
 * @param IP - the IP address to be formated <return>
 *
 * @return - a boolean value that indicates if the formating succeeds
 */
bool BackendStorer::formatIP_(std::string &IP) {	
	/*remove all white spaces*/
	for (size_t i = 0; i < IP.size(); i++) {
		if (IP[i] == ' ') IP.erase(i, 1);
	}

	/*check if it is empty*/
	if (IP.empty()) {
		fprintf(stderr, "Error: the IP is empty!\n");
		return 0;
	}

	return 1;
}

/*
 * check if two directory names are equivalent
 *
 * @param dirName1 - the first directory name
 * @param dirName2 - the second directory name
 *
 * @return - a boolean value that indicates if the two directory names are equivalent
 */
bool BackendStorer::equDirName_(const std::string &dirName1, const std::string &dirName2) {
	/*get the current directory name*/
	std::string currDir = get_current_dir_name();
	if (currDir[currDir.size() - 1] != '/') {
		currDir += '/';
	}

	std::string stdDirName1, stdDirName2;
	/*standardize the first directory name*/
	if (dirName1[0] == '/') {	
		stdDirName1 = dirName1;	
	}
	else {
		if (dirName1.substr(0, 2) != "./") {
			stdDirName1 = currDir + dirName1.substr(2);
		}
		else {
			stdDirName1 = currDir + dirName1;
		}
	}

	/*standardize the second directory name*/
	if (dirName2[0] == '/') {	
		stdDirName2 = dirName2;	
	}
	else {	
		if (dirName2.substr(0, 2) != "./") {
			stdDirName2 = currDir + dirName2.substr(2);
		}
		else {
			stdDirName2 = currDir + dirName2;
		}
	}

	/*compare the standardized directory names*/
	if (stdDirName1 == stdDirName2) {
		return 1;
	}
	else {
		return 0;
	}
}

/*
 * shorten a full file name into a short file name
 *
 * @param fullFileName - the full file name (including the path)
 * @param shortFileName - the short file name (excluding the path) <return>
 *
 * @return - a boolean value that indicates if the shortening op succeeds
 */
bool BackendStorer::shortenFileName_(const std::string &fullFileName, std::string &shortFileName) {
	std::string dirName;
	size_t currPos;

	if ((currPos = fullFileName.rfind('/')) != std::string::npos) {
		dirName = fullFileName.substr(0, currPos + 1); 	
		shortFileName = fullFileName.substr(currPos + 1); 

		if (!equDirName_(dirName, cacheDirName_)) {
			fprintf(stderr, "Error: pls check the correctness of the setting with cacheDirName_ = '%s'!\n", cacheDirName_.c_str());
			return 0;
		}

		return 1;		
	}
	else {
		fprintf(stderr, "Error: the file name '%s' is not a full file name (including the path)!\n", fullFileName.c_str());
		return 0;
	}
}

/*
 * check if a file exists
 *
 * @param fullFileName - the full file name
 *
 * @return - a boolean value that indicates if the file exists
 */
inline bool BackendStorer::checkFileExistence_(const std::string &fullFileName) {
	return access(fullFileName.c_str(), F_OK) == 0;
}

/*
 * check if a file can be accessed with all permissions
 *
 * @param fullFileName - the full file name
 *
 * @return - a boolean value that indicates if the file can be accessed with all permissions
 */
inline bool BackendStorer::checkFilePermissions_(const std::string &fullFileName) {
	/*read, write, and execute permissions*/
	return access(fullFileName.c_str(), R_OK|W_OK) == 0;
}

/*
 * get the size of an opened file
 *
 * @param fp - the file pointer
 *
 * @return - the size of the file
 */
inline long BackendStorer::getOpenedFileSize_(FILE *fp) {
	long prevPos=ftell(fp);

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);

	fseek(fp, prevPos, SEEK_SET); 

	return size;
}

/*
 * get the size of an unopened file
 *
 * @param fullFileName - the full file name
 *
 * @return - the size of the file
 */
inline long BackendStorer::getUnopenedFileSize_(const std::string &fullFileName) {
	FILE *fp =fopen(fullFileName.c_str(), "rb");
	if (fp == NULL) {
		fprintf(stderr, "Error: fail to open the file '%s' for getting its size!\n", fullFileName.c_str());
		return 0;	
	}	

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);

	fclose(fp);

	return size;
}

/*
 * locally copy data from the file with srcFullFileName to the file with destFullFileName
 *
 * @param destFullFileName - the full name of the destination file
 * @param srcFullFileName - the full name of the source file
 *
 * @return - a boolean value that indicates if the copy op succeeds
 */
bool BackendStorer::localCopyFile_(const std::string &destFullFileName, const std::string &srcFullFileName) {
	FILE *fpSrc =fopen(srcFullFileName.c_str(), "rb");
	if (fpSrc == NULL) {
		fprintf(stderr, "Error: fail to open the file '%s' for reading data!\n", srcFullFileName.c_str());
		return 0;	
	}	

	FILE *fpDest =fopen(destFullFileName.c_str(), "wb");
	if (fpDest == NULL) {
		fprintf(stderr, "Error: fail to open the file '%s' for writing data!\n", destFullFileName.c_str());

		fclose(fpSrc);

		return 0;	
	}

	char buffer[4096]; /*4KB buffer*/
	size_t len;

	while ((len = fread(buffer, sizeof(char), 4096, fpSrc)) > 0) {
		if (fwrite(buffer, sizeof(char), len, fpDest) != len) {
			fprintf(stderr, "Error: incompletely copy data from the file '%s' to the file '%s'!\n", 
					srcFullFileName.c_str(), destFullFileName.c_str());

			fclose(fpSrc);
			fclose(fpDest);

			return 0;	
		}
	}

	fclose(fpSrc);
	fclose(fpDest);

	return 1;
}

/*
 * IP-based copy data from the file with srcFullFileName to the file with destFullFileName
 *
 * @param destIP - the IP of the destination server
 * @param destFullFileName - the full name of the destination file
 * @param srcIP - the IP of the source server
 * @param srcFullFileName - the full name of the source file
 *
 * @return - a boolean value that indicates if the copy op succeeds
 */
bool BackendStorer::IPCopyFile_(const std::string &destIP, const std::string &destFullFileName, 
		const std::string &srcIP, const std::string &srcFullFileName) {

	/*use scp to copy the file*/

	return 1;
}

/*
 * store a new file into the backend storage 
 *
 * @return - a boolean value that indicates if the store op succeeds
 */
bool BackendStorer::backendStoreNewFile_(const std::string &shortFileName) {
	if (backendType_ == IPSERVER) {
		if (backendIP_ == "127.0.0.1") {
			if (!localCopyFile_(backendDirName_ + shortFileName, cacheDirName_ + shortFileName)) {			
				fprintf(stderr, "Error: fail to store the file '%s' from the cache '%s' to the backend storage '%s'!\n", 
						shortFileName.c_str(), cacheDirName_.c_str(), backendDirName_.c_str());
				return 0;	
			}
		}
		else {
			if (!IPCopyFile_(backendIP_, backendDirName_ + shortFileName, "", cacheDirName_ + shortFileName)) {			
				fprintf(stderr, "Error: fail to store the file '%s' from the local cache '%s' to the backend storage '%s' with IP '%s'!\n", 
						shortFileName.c_str(), cacheDirName_.c_str(), backendDirName_.c_str(), backendIP_.c_str());
				return 0;	
			}
		}
	}
	if (backendType_ == AMAZON) {
	}
	if (backendType_ == GOOGLE) {
	}
	if (backendType_ == MSAZURE) {
	}
	if (backendType_ == RACKSPACE) {
	}
	return 1;
}

/*
 * restore a new file from the backend storage 
 *
 * @return - a boolean value that indicates if the restore op succeeds
 */
bool BackendStorer::backendRestoreOldFile_(const std::string &shortFileName, FILE *&fp) {
	if (backendType_ == IPSERVER) {
		if (backendIP_ == "127.0.0.1") {		
			if (!localCopyFile_(cacheDirName_ + shortFileName, backendDirName_ + shortFileName)) {			
				fprintf(stderr, "Error: fail to restore the file '%s' from the backend storage '%s' to the cache '%s'!\n", 
						shortFileName.c_str(), backendDirName_.c_str(), cacheDirName_.c_str());
				return 0;	
			}
		}
		else {	
			if (!IPCopyFile_("", cacheDirName_ + shortFileName, backendIP_, backendDirName_ + shortFileName)) {			
				fprintf(stderr, "Error: fail to restore the file '%s' from the backend storage '%s' with IP '%s' to the local cache '%s'!\n", 
						shortFileName.c_str(), backendDirName_.c_str(), backendIP_.c_str(), cacheDirName_.c_str());
				return 0;	
			}
		}
	}
	if (backendType_ == AMAZON) {
	}
	if (backendType_ == GOOGLE) {
	}
	if (backendType_ == MSAZURE) {
	}
	if (backendType_ == RACKSPACE) {
	}
	return 1;
}

/*
 * add a new file
 *
 * @param fullFileName - the full file name
 *
 * @return - a boolean value that indicates if the add op succeeds
 */
bool BackendStorer::addNewFile(const std::string &fullFileName) {
	std::string shortFileName;
	fileNode_t *targetFileNode;
	long fileSize;

	/*1. get the file size*/
	fileSize = getUnopenedFileSize_(fullFileName);

	/*2. update availCacheSize_, and signal the cache updater if necessary*/	
	pthread_mutex_lock(&cacheUpdaterLock_);	
	/*update availCacheSize_*/
	availCacheSize_ -= fileSize;
	/*signal the cache updater if necessary*/
	if (availCacheSize_ < usedCacheSize_) {
		pthread_cond_signal(&cacheUpdaterCond_);
	}
	pthread_mutex_unlock(&cacheUpdaterLock_);

	/*3. shorten the file name*/
	if (!shortenFileName_(fullFileName, shortFileName)) {
		fprintf(stderr, "Error: fail to shorten the full file name '%s'!\n", fullFileName.c_str());
		return 0;
	}

	/*4. create a new file node for the file*/
	targetFileNode = (fileNode_t *) malloc(fileNodeSize_);
	strcpy(targetFileNode->shortFileName, shortFileName.c_str());
	targetFileNode->fileSize = fileSize;
	targetFileNode->next = NULL;

	/*5. add the file node to the tail of the newly stored file link, and then signal the file storer if necessary*/
	pthread_mutex_lock(&fileStorerLock_);
	/*add the file node to the tail of the newly stored file link*/
	if (tailNewlyStoredFileNode_ == NULL) {
		headNewlyStoredFileNode_ = targetFileNode;
		tailNewlyStoredFileNode_ = targetFileNode;
	}
	else {
		tailNewlyStoredFileNode_->next = targetFileNode;
		tailNewlyStoredFileNode_ = targetFileNode;		
	}
	numOfNewlyStoredFiles_++;
	/*signal the file storer if necessary*/
	if (numOfNewlyStoredFiles_ > 0) {
		pthread_cond_signal(&fileStorerCond_);
	}
	pthread_mutex_unlock(&fileStorerLock_);

	return 1;	
}

/*
 * open an old file
 *
 * @param fullFileName - the full file name
 * @param fp - the file pointer <return>
 *
 * @return - a boolean value that indicates if the open op succeeds
 */
bool BackendStorer::openOldFile(const std::string &fullFileName, FILE *&fp) {
	std::string shortFileName;
	fileNode_t *targetFileNode;
	long fileSize;

	/*shorten the file name*/
	if (!shortenFileName_(fullFileName, shortFileName)) {
		fprintf(stderr, "Error: fail to shorten the full file name '%s'!\n", fullFileName.c_str());
		return 0;
	}

	/*if this file is in the local dir*/
	if (checkFileExistence_(fullFileName)) {
		/*1. check file access permissions*/
		if (!checkFilePermissions_(fullFileName)) {
			fprintf(stderr, "Error: do not have all permissions to access the existing file '%s'!\n", fullFileName.c_str());	
			return 0;	
		}

		/*2. open the file*/
		fp = fopen(fullFileName.c_str(), "rb");
		if (fp == NULL) {
			fprintf(stderr, "Error: fail to open the existing file '%s'!\n", fullFileName.c_str());		
			return 0;	
		}

		/*3. get the file size*/
		fileSize = getOpenedFileSize_(fp);

		/*4. create a new file node for the file*/
		targetFileNode = (fileNode_t *) malloc(fileNodeSize_);
		strcpy(targetFileNode->shortFileName, shortFileName.c_str());
		targetFileNode->fileSize = fileSize;
		targetFileNode->next = NULL;

		/*5. add the file node to the tail of the newly used file link, and then signal the cache updater*/
		pthread_mutex_lock(&cacheUpdaterLock_);
		/*add the file node to the tail of the newly used file link*/
		if (tailNewlyUsedFileNode_ == NULL) {
			headNewlyUsedFileNode_ = targetFileNode;
			tailNewlyUsedFileNode_ = targetFileNode;
		}
		else {
			tailNewlyUsedFileNode_->next = targetFileNode;
			tailNewlyUsedFileNode_ = targetFileNode;		
		}
		numOfNewlyUsedFiles_++;
		/*signal the cache updater if necessary*/
		if (numOfNewlyUsedFiles_ > 0) {
			pthread_cond_signal(&cacheUpdaterCond_);
		}
		pthread_mutex_unlock(&cacheUpdaterLock_);	
	}
	/*if this file is in the backend storage*/
	else {
		/*1. restore the file from the backend storage*/
		if (!backendRestoreOldFile_(shortFileName, fp)) {
			fprintf(stderr, "Error: fail to restore an old file '%s' from the backend storage!\n", shortFileName.c_str());	
			return 0;	
		}

		/*2. get the file size*/
		fileSize = getOpenedFileSize_(fp);

		/*3. create a new file node for the file*/
		targetFileNode = (fileNode_t *) malloc(fileNodeSize_);
		strcpy(targetFileNode->shortFileName, shortFileName.c_str());
		targetFileNode->fileSize = fileSize;
		targetFileNode->next = NULL;

		/*4. add the file node to the tail of the newly used file link, and then signal the cache updater*/
		pthread_mutex_lock(&cacheUpdaterLock_);
		/*add the file node to the tail of the newly used file link*/
		if (tailNewlyUsedFileNode_ == NULL) {
			headNewlyUsedFileNode_ = targetFileNode;
			tailNewlyUsedFileNode_ = targetFileNode;
		}
		else {
			tailNewlyUsedFileNode_->next = targetFileNode;
			tailNewlyUsedFileNode_ = targetFileNode;		
		}
		numOfNewlyUsedFiles_++;
		usedCacheSize_ += targetFileNode->fileSize;
		/*signal the cache updater if necessary*/
		if ((numOfNewlyUsedFiles_ > 0) || (availCacheSize_ < usedCacheSize_)) {
			pthread_cond_signal(&cacheUpdaterCond_);
		}
		pthread_mutex_unlock(&cacheUpdaterLock_);	
	}

	return 1;
}

/*
 * store each new file into the backend storage 
 */
void BackendStorer::storeNewFile() {
	std::string shortFileName;
	fileNode_t *targetFileNode;

	while (1) {
		/*1. get the first file node from the head of the newly stored file link if numOfNewlyStoredFiles_ > 0*/
		pthread_mutex_lock(&fileStorerLock_);
		/*wait until numOfNewlyStoredFiles_ > 0*/
		while (numOfNewlyStoredFiles_ <= 0) {
			pthread_cond_wait(&fileStorerCond_, &fileStorerLock_);
		}		
		/*get the first file node from the head of the newly stored file link*/
		targetFileNode = headNewlyStoredFileNode_;
		if (headNewlyStoredFileNode_ == tailNewlyStoredFileNode_) {
			headNewlyStoredFileNode_ = NULL;
			tailNewlyStoredFileNode_ = NULL;
		}
		else {
			headNewlyStoredFileNode_ = headNewlyStoredFileNode_->next;
		}
		numOfNewlyStoredFiles_--;	
		pthread_mutex_unlock(&fileStorerLock_);

		/*2. store the file into the backend storage*/
		if (!backendStoreNewFile_(targetFileNode->shortFileName)) {
			fprintf(stderr, "Error: fail to store a file '%s' into the backend storage!\n", shortFileName.c_str());	
			exit(1);	
		}

		/*3. add the file node to the tail of the newly used file link, and then signal the cache updater*/
		pthread_mutex_lock(&cacheUpdaterLock_);
		/*add the file node to the tail of the newly used file link*/
		targetFileNode->next = NULL;
		if (tailNewlyUsedFileNode_ == NULL) {
			headNewlyUsedFileNode_ = targetFileNode;
			tailNewlyUsedFileNode_ = targetFileNode;
		}
		else {
			tailNewlyUsedFileNode_->next = targetFileNode;
			tailNewlyUsedFileNode_ = targetFileNode;		
		}
		numOfNewlyUsedFiles_++;
		availCacheSize_ += targetFileNode->fileSize;
		usedCacheSize_ -= targetFileNode->fileSize;
		/*signal the cache updater if necessary*/
		if (numOfNewlyUsedFiles_ > 0) {
			pthread_cond_signal(&cacheUpdaterCond_);
		}
		pthread_mutex_unlock(&cacheUpdaterLock_);
	}
}

/*
 * update the status of the cache
 */
void BackendStorer::updateCache() {
	std::string shortFileName, fullFileName;
	fileNode_t *targetFileNode;
	long fileSize;
	lIterator_t lIterator;
	rIterator_t rIterator;

	while (1) {
		pthread_mutex_lock(&cacheUpdaterLock_);

		while ((numOfNewlyUsedFiles_ <= 0) && (availCacheSize_ >= usedCacheSize_)) { 
			pthread_cond_wait(&cacheUpdaterCond_, &cacheUpdaterLock_);
		}

		/*1. add a cache entry for each file recored in the newly used file link*/
		while (numOfNewlyUsedFiles_ > 0) {				
			/*get the first file node from the head of the newly used file link*/
			targetFileNode = headNewlyUsedFileNode_;
			if (headNewlyUsedFileNode_ == tailNewlyUsedFileNode_) {
				headNewlyUsedFileNode_ = NULL;
				tailNewlyUsedFileNode_ = NULL;
			}
			else {
				headNewlyUsedFileNode_ = headNewlyUsedFileNode_->next;
			}

			/*update the cache for this file*/
			lIterator = cache_.left.find(targetFileNode->shortFileName);			
			/*if an old cache entry does not exist, append a new one to the tail*/
			if (lIterator == cache_.left.end()) {
				cache_.insert(cacheEntry_t(targetFileNode->shortFileName, targetFileNode->fileSize));
			}
			/*if an old cache entry exists, move it to the tail*/
			else {
				cache_.right.relocate(cache_.right.end(), cache_.project_right(lIterator));
			}

			/*free the file node*/
			free(targetFileNode);

			/*update numOfNewlyUsedFiles_*/
			numOfNewlyUsedFiles_--;
		}

		/*2. evict some files if the cache is full*/
		while (availCacheSize_ < usedCacheSize_) {
			/*evict the least recently used file from the cache*/
			rIterator = cache_.right.begin();
			fileSize = rIterator->first;
			shortFileName = rIterator->second;
			cache_.right.erase(rIterator); 

			/*delete the file from the cache dir*/
			fullFileName = cacheDirName_ + shortFileName;
			remove(fullFileName.c_str());

			/*update usedCacheSize_*/
			usedCacheSize_ -= fileSize;			
		}

		pthread_mutex_unlock(&cacheUpdaterLock_);
	}
}

