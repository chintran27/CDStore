/*
 * DedupCore.cc
 */

#include "DedupCore.hh"

using namespace std;

/*
 * constructor of DedupCore
 *
 * @param dedupDirName - the name of the directory that stores all deduplication-related data
 * @param dbDirName - the name of the directory that stores the key-value database
 * @param recipeFileDirName - the name of the directory that stores the recipe files
 * @param shareContainerDirName - the name of the directory that stores the share containers
 * @param recipeStorerObj - the BackendStorer instance that manages recipe files
 * @param containerStorerObj - the BackendStorer instance that manages share containers 
 */
DedupCore::DedupCore(const std::string &dedupDirName, const std::string &dbDirName, 
		const std::string &recipeFileDirName, const std::string &shareContainerDirName, 
		BackendStorer *recipeStorerObj, BackendStorer *containerStorerObj) {
	dedupDirName_ = dedupDirName;
	dbDirName_ = dbDirName;
	recipeFileDirName_ = recipeFileDirName;
	shareContainerDirName_ = shareContainerDirName;

	recipeStorerObj_ = recipeStorerObj;
	containerStorerObj_ = containerStorerObj;

	/*format all input dir names*/
	if (!formatDirName_(dedupDirName_)) {
		fprintf(stderr, "Error: the input 'dedupDirName' is invalid!\n");
		exit(1);	
	}	
	if (!formatDirName_(dbDirName_)) {
		fprintf(stderr, "Error: the input 'dbDirName' is invalid!\n");
		exit(1);	
	}
	if (!formatDirName_(recipeFileDirName_)) {
		fprintf(stderr, "Error: the input 'recipeFileDirName' is invalid!\n");
		exit(1);	
	}
	if (!formatDirName_(shareContainerDirName_)) {
		fprintf(stderr, "Error: the input 'shareContainerDirName' is invalid!\n");
		exit(1);	
	}

	/*create the DB dir*/
	if (!addPrefixDir_(dedupDirName_, dbDirName_)) {
		fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", dedupDirName_.c_str(), dbDirName_.c_str());
		exit(1);	
	}	
	if (!createDir_(dbDirName_)) {
		fprintf(stderr, "Error: fail to create the dir '%s'!\n", dbDirName_.c_str());
		exit(1);	
	}	

	/*create the recipe file dir*/
	if (!addPrefixDir_(dedupDirName_, recipeFileDirName_)) {
		fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", dedupDirName_.c_str(), recipeFileDirName_.c_str());
		exit(1);	
	}	
	if (!createDir_(recipeFileDirName_)) {
		fprintf(stderr, "Error: fail to create the dir '%s'!\n", recipeFileDirName_.c_str());
		exit(1);	
	}

	/*create the share container dir*/
	if (!addPrefixDir_(dedupDirName_, shareContainerDirName_)) {
		fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", dedupDirName_.c_str(), shareContainerDirName_.c_str());
		exit(1);	
	}
	if (!createDir_(shareContainerDirName_)) {
		fprintf(stderr, "Error: fail to create the dir '%s'!\n", shareContainerDirName_.c_str());
		exit(1);	
	}	

	/*open/create the key-value database*/
	dbOptions_.create_if_missing = true;
	dbOptions_.write_buffer_size = MEM_TABLE_SIZE;
	dbOptions_.block_cache = leveldb::NewLRUCache(BLOCK_CACHE_SIZE);
	dbOptions_.filter_policy = leveldb::NewBloomFilterPolicy(BLOOM_FILTER_KEY_BITS);
	leveldb::Status openStat = leveldb::DB::Open(dbOptions_, dbDirName_, &db_);
	if (openStat.ok() == false) {
		fprintf(stderr, "Error: fail to open/create the database '%s'!\n", dbDirName_.c_str());
		fprintf(stderr, "Status: %s \n", openStat.ToString().c_str());
		exit(1);	
	}	

	fileShareMDHeadSize_ = sizeof(fileShareMDHead_t);
	shareMDEntrySize_ = sizeof(shareMDEntry_t);

	inodeIndexValueHeadSize_ = sizeof(inodeIndexValueHead_t);
	inodeDirEntrySize_ = sizeof(inodeDirEntry_t);
	inodeFileEntrySize_ = sizeof(inodeFileEntry_t);

	shareIndexValueHeadSize_ = sizeof(shareIndexValueHead_t);
	shareUserRefEntrySize_ = sizeof(shareUserRefEntry_t);

	/*initialize the file recipe name fileRecipeName_*/
	recipeFileNameValidLen_ = INTERNAL_FILE_NAME_SIZE - 4;
	std::string recipeFileNameMain(recipeFileNameValidLen_, 'a');
	std::string recipeFileNamePostfix(".rf"); /*an implicit null-terminator*/
	globalRecipeFileName_ = recipeFileNameMain + recipeFileNamePostfix;

	fileRecipeHeadSize_ = sizeof(fileRecipeHead_t);
	fileRecipeEntrySize_ = sizeof(fileRecipeEntry_t);

	/*initialize the share container name shareContainerName_*/
	shareContainerNameValidLen_ = INTERNAL_FILE_NAME_SIZE - 4;
	std::string shareContainerNameMain(shareContainerNameValidLen_, 'a');
	std::string shareContainerNamePostfix(".sc"); /*an implicit null-terminator*/
	globalShareContainerName_ = shareContainerNameMain + shareContainerNamePostfix;

	/*initialize the head and tail nodes of the file recipe pool*/
	headBufferNode_ = NULL;
	tailBufferNode_ = NULL;

	perUserBufferNodeSize_ = sizeof(perUserBufferNode_t);

	shareFileHeadSize_ = sizeof(shareFileHead_t);
	shareEntrySize_ = sizeof(shareEntry_t);

	/*initialize the mutex lock DBLock_*/
	if (pthread_mutex_init(&DBLock_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the mutex lock DBLock_!\n");
		exit(1);	
	}		

	/*initialize the mutex lock bufferLock_*/
	if (pthread_mutex_init(&bufferLock_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the mutex lock bufferLock_!\n");
		exit(1);	
	}		

	/*initialize the mutex lock globalRecipeFileNameLock_*/
	if (pthread_mutex_init(&globalRecipeFileNameLock_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the mutex lock globalRecipeFileNameLock_!\n");
		exit(1);	
	}		

	/*initialize the mutex lock globalShareContainerNameLock_*/
	if (pthread_mutex_init(&globalShareContainerNameLock_, NULL) != 0) {
		fprintf(stderr, "Error: fail to initialize the mutex lock globalShareContainerNameLock_!\n");
		exit(1);	
	}	

	fprintf(stderr, "\nA DedupCore has been constructed! \n");		
	fprintf(stderr, "Parameters: \n");		
	fprintf(stderr, "      dbDirName_: %s \n", dbDirName_.c_str());		
	fprintf(stderr, "      recipeFileDirName_: %s \n", recipeFileDirName_.c_str());
	fprintf(stderr, "      shareContainerDirName_: %s \n", shareContainerDirName_.c_str());
	fprintf(stderr, "\n");	
}

/* 
 * destructor of DedupCore 
 */
DedupCore::~DedupCore() {
	/*close the key-value database*/
	delete db_;
	delete dbOptions_.block_cache;
	delete dbOptions_.filter_policy;

	/*clean up the buffer node link*/
	if (!cleanupAllBufferNodes()) {
		fprintf(stderr, "Warning: fail to clean up the buffer node link!\n");
	}

	/*clean up the mutex lock DBLock_*/	
	pthread_mutex_destroy(&DBLock_);

	/*clean up the mutex lock bufferLock_*/	
	pthread_mutex_destroy(&bufferLock_);

	/*clean up the mutex lock globalRecipeFileNameLock_*/	
	pthread_mutex_destroy(&globalRecipeFileNameLock_);

	/*clean up the mutex lock globalShareContainerNameLock_*/	
	pthread_mutex_destroy(&globalShareContainerNameLock_);

	fprintf(stderr, "\nThe DedupCore has been destructed! \n");	
	fprintf(stderr, "\n");
}

/*
 * format a full file name (including the path) into '/.../.../shortName'
 *
 * @param fullFileName - the full file name to be formated <return>
 *
 * @return - a boolean value that indicates if the formating succeeds
 */
bool DedupCore::formatFullFileName_(std::string &fullFileName) {
	/*check if it is empty*/
	if (fullFileName.empty()) {
		fprintf(stderr, "Error: the full file name is empty!\n");
		return 0;
	}

	/*make sure that fullFileName starts with '/'*/
	if (fullFileName[0] != '/') {
		if ((fullFileName.substr(0, 2) == "./") || (fullFileName.substr(0, 3) == "../")) {
			fprintf(stderr, "Error: the full file name ('%s') should not begin with './' or '../'!\n", fullFileName.c_str());
			return 0;
		}
		else {
			fullFileName = '/' + fullFileName;
		}
	}	

	return 1;
}

/*
 * format a directory name into '.../.../shortName/'
 *
 * @param dirName - the directory name to be formated <return>
 *
 * @return - a boolean value that indicates if the formating succeeds
 */
bool DedupCore::formatDirName_(std::string &dirName) {
	/*check if it is empty*/
	if (dirName.empty()) {
		fprintf(stderr, "Error: the dir name is empty!\n");
		return 0;
	}

	/*make sure that dirName ends with '/'*/
	if (dirName[dirName.size() - 1] != '/') {
		dirName += '/';
	}	

	return 1;
}

/*
 * prefix a (directory or file) name with a directory name
 *
 * @param prefixDirName - the prefix directory name to be formated
 * @param name - the (directory or file) name to be prefixed <return>
 *
 * @return - a boolean value that indicates if the prefixing succeeds
 */
bool DedupCore::addPrefixDir_(const std::string &prefixDirName, std::string &name) {
	if ((name.substr(0, 1) == "/") || (name.substr(0, 2) == "./") || (name.substr(0, 3) == "../")) {
		fprintf(stderr, "Error: cannot add a prefix dir for the name ('%s'), since it begins with '/', './', or '../'!\n", name.c_str());
		return 0;
	}

	name = prefixDirName + name;

	return 1;
}

/*
 * check if a file (or directory) is accessible
 *
 * @param name - the name of the file to be checked
 *
 * @return - a boolean value that indicates if the file is accessible
 */
inline bool DedupCore::checkFileAccess_(const std::string &name) {
	return checkFileExistence_(name) && checkFilePermissions_(name);
}

/*
 * check if a file (or directory) exists
 *
 * @param name - the name of the file to be checked
 *
 * @return - a boolean value that indicates if the file exists
 */
inline bool DedupCore::checkFileExistence_(const std::string &name) {
	return access(name.c_str(), F_OK) == 0;
}

/*
 * check if a file (or directory) can be accessed with all permissions
 *
 * @param name - the name of the file to be checked
 *
 * @return - a boolean value that indicates if the file can be accessed with all permissions
 */
inline bool DedupCore::checkFilePermissions_(const std::string &name) {
	/*read, write, and execute permissions*/
	return access(name.c_str(), R_OK|W_OK|X_OK) == 0;
}

/*
 * get the size of an opened file
 *
 * @param fp - the file pointer
 *
 * @return - the size of the file
 */
inline long DedupCore::getFileSize_(FILE *fp) {
	long prevPos=ftell(fp);

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);

	fseek(fp, prevPos, SEEK_SET); 

	return size;
}

/*
 * recursively create a directory
 *
 * @param dirName - the name (which ends with '/') of the directory to be created
 *
 * @return - a boolean value that indicates if the create op succeeds
 */
bool DedupCore::createDir_(const std::string &dirName) {
	mode_t mode = 0755;
	std::string subDir;
	size_t currPos, prePos;

	/*note: the dir name should end with '/' (by formatDirName_())*/

	/*recursively create the dir*/
	prePos = 0;
	while ((currPos = dirName.find('/', prePos)) != std::string::npos) {
		subDir = dirName.substr(0, currPos + 1); 	
		if ((mkdir(subDir.c_str(), mode) != 0) && (errno != EEXIST)){
			/*if mkdir fails, and the dir does not exist*/
			return 0;
		}

		if (!checkFilePermissions_(dirName)) {
			/*if the dir does not grant read, write, and execute permissions*/
			return 0;
		}

		prePos = currPos + 1;
	}

	return 1;
}

/*
 * increment a global file name in lexicographical order
 *
 * @param fileName - the file name to be incremented <return>
 * @param validLen - the length of the file name excluding the suffix
 *
 * @return - a boolean value that indicates if the incrementing succeeds
 */
bool DedupCore::incrGlobalFileName_(std::string &fileName, const int &validLen) {
	int i = validLen - 1;

	/*we use only 'a' - 'z' in the name of an archive file (like file recipe file or share container file) */

	while ((i >= 0) && (fileName[i] == 'z')) {
		i--;
	}

	if (i == -1) {
		return 0;
	}

	fileName[i]++;

	i++;
	while (i < validLen) {
		fileName[i] = 'a';
		i++;
	}

	return 1;	
}

/*
 * transform a file name to an inode's fingerprint
 *
 * @param fullFileName - the full file name to be transformed 
 * @param userID - the user id
 * @param inodeFP - the resulting inode's fingerprint <return>
 * @param cryptoObj - the CryptoPrimitive instance for calculating hash fingerprint
 *
 * @return - a boolean value that indicates if the transformation succeeds
 */
inline bool DedupCore::fileName2InodeFP_(const std::string &fullFileName, const int &userID, char *inodeFP, 
		CryptoPrimitive *cryptoObj) {
	unsigned char *hashInputBuffer;
	bool returnValue;	

	/*generate the inode's fingerprint from both fullFileName and userID*/
	hashInputBuffer = (unsigned char *) malloc(fullFileName.size() + sizeof(int));
	memcpy(hashInputBuffer, fullFileName.data(), fullFileName.size());
	memcpy(hashInputBuffer + fullFileName.size(), &userID, sizeof(int));
	returnValue = cryptoObj->generateHash(hashInputBuffer, fullFileName.size() + sizeof(int), (unsigned char *) inodeFP);

	free(hashInputBuffer);

	return returnValue;
}

/*
 * transform an inode's fingerprint to an index key
 *
 * @param inodeFP - the inode's fingerprint to be transformed 
 * @param key - the resulting index key <return>
 */
inline void DedupCore::inodeFP2IndexKey_(char *inodeFP, char *key) {
	/*set the key to be the inode's fingerprint*/
	memcpy(key + 1, inodeFP, FP_SIZE);
	/*add a prefix '0' for indicating inode index*/
	key[0] = '0';
}

/*
 * transform a share's fingerprint to an index key
 *
 * @param shareFP - the share's fingerprint to be transformed 
 * @param key - the resulting index key <return>
 */
inline void DedupCore::shareFP2IndexKey_(char *shareFP, char *key) {
	/*set the key to be the share's fingerprint*/
	memcpy(key + 1, shareFP, FP_SIZE);
	/*add a prefix '1' for indicating share index*/
	key[0] = '1'; 
}

/*
 * get the current time (in second)
 *
 * @param time - the current time <return>
 */
inline void DedupCore::getCurrTime_(double &time) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	time = (double) tv.tv_sec + (double) tv.tv_usec * 1e-6;
}

/*
 * flush a buffer node into the disk
 *
 * @param targetBufferNode - the buffer node to be flushed
 *
 * @return - a boolean value that indicates if the flush op succeeds
 */
bool DedupCore::flushBufferNodeIntoDisk_(perUserBufferNode_t *targetBufferNode) {
	FILE *fp;

	/*flush the content of recipeFileBuffer into the disk*/
	if (targetBufferNode->recipeFileBufferCurrLen > 0) {
		/*if the last file recipe head is at the beginning of the buffer*/
		if (targetBufferNode->lastRecipeHeadPos == 0) {
			std::string recipeFileName;
			int recipeFileOffset;

			/*find the old recipe file info in the database*/
			if (!findOldRecipeFile_(targetBufferNode, recipeFileName, recipeFileOffset)) {	
				fprintf(stderr, "Error: fail to find the old recipe file info in the database!\n");
				return 0;		
			}

			if (!addPrefixDir_(recipeFileDirName_, recipeFileName)) {
				fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", recipeFileDirName_.c_str(), recipeFileName.c_str());
				return 0;	
			}

			/*open a reciple file for appending recipes; if it does not exist, then create it*/
			fp = fopen(recipeFileName.c_str(), "ab+");
			if (fp == NULL) {
				fprintf(stderr, "Error: fail to open/create the file '%s' for appending/writing recipes!\n", recipeFileName.c_str());
				return 0;
			}

			long fileSize = getFileSize_(fp);			
			/*if the file is a newly created one of size 0, directly write the data of the recipe file buffer into the file*/
			if (fileSize == 0) {	
				/*close and re-open the file in a proper mode*/
				fclose(fp);
				fp = fopen(recipeFileName.c_str(), "wb");		
				if (fp == NULL) {
					fprintf(stderr, "Error: fail to re-open the file '%s' for writing recipes!\n", recipeFileName.c_str());
					return 0;
				}

				if (recipeFileOffset != 0) {
					fprintf(stderr, "Error: the inode info in the database does not match the recipe file '%s'!\n", 
							recipeFileName.c_str());

					fclose(fp);
					return 0;	
				}

				if (fwrite(targetBufferNode->recipeFileBuffer, targetBufferNode->recipeFileBufferCurrLen, 1, fp) != 1){
					fprintf(stderr, "Error: fail to write recipes into the file '%s'!\n", recipeFileName.c_str());

					fclose(fp);
					return 0;	
				}
			}
			/*if the file is an old one, append the recipes in the buffer to the file (note: this case is only possible 
			  when an incomplete file is received; otherwise, in the last step of secondStageDedup(), all recipes 
			  in the buffer will be appended to a previous recipe file)*/
			else {
				/*close and re-open the file in a proper mode*/
				fclose(fp);
				fp = fopen(recipeFileName.c_str(), "rb+");		
				if (fp == NULL) {
					fprintf(stderr, "Error: fail to re-open the file '%s' for appending recipes!\n", recipeFileName.c_str());
					return 0;	
				}

				fileRecipeHead_t InsFileRecipeHead;

				/*read the file recipe head from the recipe file*/
				fseek(fp, recipeFileOffset, SEEK_SET);
				if (fread(&InsFileRecipeHead, fileRecipeHeadSize_, 1, fp) != 1){
					fprintf(stderr, "Error: fail to read the file recipe head from the file '%s'!\n", recipeFileName.c_str());

					fclose(fp);
					return 0;	
				}	

				/*append file recipe entries to the recipe file*/
				fseek(fp, recipeFileOffset + fileRecipeHeadSize_ + fileRecipeEntrySize_ * (InsFileRecipeHead.numOfShares), 
						SEEK_SET);
				if (fwrite(targetBufferNode->recipeFileBuffer + fileRecipeHeadSize_, 
							targetBufferNode->recipeFileBufferCurrLen - fileRecipeHeadSize_, 1, fp) != 1){
					fprintf(stderr, "Error: fail to append recipe entries to the file '%s'!\n", recipeFileName.c_str());

					fclose(fp);
					return 0;	
				}

				/*update the file recipe head into the recipe file*/
				fileRecipeHead_t *pFileRecipeHead;
				pFileRecipeHead = (fileRecipeHead_t *) (targetBufferNode->recipeFileBuffer);
				InsFileRecipeHead.numOfShares += pFileRecipeHead->numOfShares;
				fseek(fp, recipeFileOffset, SEEK_SET);
				if (fwrite(&InsFileRecipeHead, fileRecipeHeadSize_, 1, fp) != 1){
					fprintf(stderr, "Error: fail to update the file recipe head into the file '%s'!\n", recipeFileName.c_str());

					fclose(fp);
					return 0;
				}
			}

			fclose(fp);

			if (recipeStorerObj_ != NULL) {
				recipeStorerObj_->addNewFile(recipeFileName);
			}
		}
		/*if the last file recipe head is not at the beginning of the buffer, store the data of the buffer into a new file*/
		else {
			std::string recipeFileName;

			recipeFileName = targetBufferNode->recipeFileName;

			if (!addPrefixDir_(recipeFileDirName_, recipeFileName)) {
				fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", recipeFileDirName_.c_str(), recipeFileName.c_str());
				return 0;	
			}

			/*create a reciple file for writing recipes*/
			fp = fopen(recipeFileName.c_str(), "wb");		
			if (fp == NULL) {
				fprintf(stderr, "Error: fail to open the file '%s' for writing recipes!\n", recipeFileName.c_str());
				return 0;
			}

			/*write the data of the recipe file buffer into a recipe file*/
			if (fwrite(targetBufferNode->recipeFileBuffer, targetBufferNode->recipeFileBufferCurrLen, 1, fp) != 1){
				fprintf(stderr, "Error: fail to write recipes into the file '%s'!\n", recipeFileName.c_str());

				fclose(fp);
				return 0;	
			}	

			fclose(fp);	

			if (recipeStorerObj_ != NULL) {
				recipeStorerObj_->addNewFile(recipeFileName);
			}
		}
	}

	/*2. flush the content of shareContainerBuffer into the disk*/
	if (targetBufferNode->shareContainerBufferCurrLen > 0) {
		std::string shareContainerName;

		shareContainerName = targetBufferNode->shareContainerName;

		if (!addPrefixDir_(shareContainerDirName_, shareContainerName)) {
			fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", shareContainerDirName_.c_str(), shareContainerName.c_str());
			return 0;		
		}	

		/*create a container file for writing shares*/
		fp = fopen(shareContainerName.c_str(), "wb");		
		if (fp == NULL) {
			fprintf(stderr, "Error: fail to open the file '%s' for writing shares!\n", shareContainerName.c_str());
			return 0;	
		}	

		/*write the data of the share container buffer into a container file*/
		if (fwrite(targetBufferNode->shareContainerBuffer, targetBufferNode->shareContainerBufferCurrLen, 1, fp) != 1){
			fprintf(stderr, "Error: fail to write shares into the file '%s'!\n", shareContainerName.c_str());

			fclose(fp);
			return 0;	
		}	

		fclose(fp);	

		if (containerStorerObj_ != NULL) {
			containerStorerObj_->addNewFile(shareContainerName);
		}
	}

	return 1;
}

/*
 * find or create a buffer node for a user in the buffer node link
 *
 * @param userID - the user id
 * @param targetBufferNode - the resulting buffer node <return>
 */
void DedupCore::findOrCreateBufferNode_(const int &userID, perUserBufferNode_t *&targetBufferNode) {	
	perUserBufferNode_t *currBufferNode, *preBufferNode;
	double currTime, preTime;
	bool goForward;

	/*get the mutex lock bufferLock_*/
	pthread_mutex_lock(&bufferLock_);

	/*find the buffer node for this user*/
	currBufferNode = headBufferNode_;
	targetBufferNode = NULL;
	preBufferNode =NULL;
	while (currBufferNode != NULL) {
		goForward = 1;

		if (currBufferNode->userID == userID) {
			getCurrTime_(currTime);			
			currBufferNode->lastUseTime = currTime;			
			targetBufferNode = currBufferNode;
		}
		else {
			preTime = currBufferNode->lastUseTime;
			getCurrTime_(currTime);

			if (currTime - preTime  > MAX_BUFFER_WAIT_SECS) {
				/* flush the buffer node into the disk*/
				if (!flushBufferNodeIntoDisk_(currBufferNode)) {
					fprintf(stderr, "Warning: fail to flush the buffer node for userID '%d' into the disk!\n", 
							currBufferNode->userID);
				}

				/*delete the buffer node from the link and then free it*/
				if (currBufferNode == headBufferNode_) {
					currBufferNode = currBufferNode->next;
					free(headBufferNode_);

					if (headBufferNode_ == tailBufferNode_) {
						/*the only buffer node is deleted*/
						headBufferNode_ = NULL;
						tailBufferNode_ = NULL;
					}
					else {
						headBufferNode_ = currBufferNode;
					}

					preBufferNode =NULL;					
				}
				else {
					preBufferNode->next = currBufferNode->next;
					free(currBufferNode);

					if (currBufferNode == tailBufferNode_) {
						currBufferNode = NULL;
						tailBufferNode_ = preBufferNode;
					}
					else {
						currBufferNode = preBufferNode->next;
					}					
				}

				goForward = 0;				
			}				
		}

		if (goForward == 1) {
			preBufferNode = currBufferNode;
			currBufferNode = currBufferNode->next;
		}
	}	

	/*release the mutex lock bufferLock_*/
	pthread_mutex_unlock(&bufferLock_);

	/*if such a buffer node does not exist, then create one for this user*/
	if (targetBufferNode == NULL) { 
		/*create the buffer node*/
		targetBufferNode = (perUserBufferNode_t *) malloc(perUserBufferNodeSize_);

		targetBufferNode->userID = userID;

		/*get the mutex lock globalRecipeFileNameLock_*/
		pthread_mutex_lock(&globalRecipeFileNameLock_);

		globalRecipeFileName_.copy(targetBufferNode->recipeFileName, INTERNAL_FILE_NAME_SIZE-1, 0);
		targetBufferNode->recipeFileName[INTERNAL_FILE_NAME_SIZE-1] = '\0';
		incrGlobalFileName_(globalRecipeFileName_, recipeFileNameValidLen_);		

		/*release the mutex lock globalRecipeFileNameLock_*/
		pthread_mutex_unlock(&globalRecipeFileNameLock_);

		targetBufferNode->recipeFileBufferCurrLen = 0;
		targetBufferNode->lastRecipeHeadPos = 0;

		/*get the mutex lock globalShareContainerNameLock_*/
		pthread_mutex_lock(&globalShareContainerNameLock_);

		globalShareContainerName_.copy(targetBufferNode->shareContainerName, INTERNAL_FILE_NAME_SIZE - 1, 0);
		targetBufferNode->shareContainerName[INTERNAL_FILE_NAME_SIZE - 1] = '\0';
		incrGlobalFileName_(globalShareContainerName_, recipeFileNameValidLen_);		

		/*release the mutex lock globalShareContainerNameLock_*/
		pthread_mutex_unlock(&globalShareContainerNameLock_);

		targetBufferNode->shareContainerBufferCurrLen = 0;

		getCurrTime_(currTime);
		targetBufferNode->lastUseTime = currTime;

		targetBufferNode->next = NULL;

		/*get the mutex lock bufferLock_*/
		pthread_mutex_lock(&bufferLock_);

		/*link it in the end of the pool*/
		if (headBufferNode_ == NULL) {
			headBufferNode_ = targetBufferNode;
			tailBufferNode_= targetBufferNode;
		}
		else {
			tailBufferNode_->next = targetBufferNode;
			tailBufferNode_ = targetBufferNode;
		}

		/*release the mutex lock bufferLock_*/
		pthread_mutex_unlock(&bufferLock_);
	}		
}

/*
 * add a file's information into the inode index
 *
 * @param fullFileName - the full name of the file to be added 
 * @param userID - the user id 
 * @param targetBufferNode - the corresponding buffer node 
 * @param cryptoObj - the CryptoPrimitive instance for calculating hash fingerprint
 *
 * @return - a boolean value that indicates if the add op succeeds
 */
bool DedupCore::addFileIntoInodeIndex_(const std::string &fullFileName, const int &userID, 
		perUserBufferNode_t *targetBufferNode, CryptoPrimitive *cryptoObj) {
	leveldb::Status fileStat, dirStat;
	std::string shortName, dirName;
	size_t currPos;
	char currFP[FP_SIZE], preFP[FP_SIZE];	
	char key[KEY_SIZE], *value;
	leveldb::Slice *fileKeySlice, *dirKeySlice, *valueSlice;
	std::string valueString;
	int valueSize, valueOffset;
	inodeIndexValueHead_t *pInodeIndexValueHead;
	inodeFileEntry_t *pInodeFileEntry;
	inodeDirEntry_t *pInodeDirEntry;
	bool inPathFlag;
	int i;

	/*fullFileName always has the format '/.../.../shortName'*/	
	currPos = fullFileName.rfind('/');
	/*for the fullFileName, shortName does not end with '/'*/
	shortName = fullFileName.substr(currPos + 1);
	/*dirName always ends with '/'*/
	dirName = fullFileName.substr(0, currPos + 1); 

	/*generate the key*/
	fileName2InodeFP_(fullFileName, userID, currFP, cryptoObj);	
	inodeFP2IndexKey_(currFP, key); 
	fileKeySlice = new leveldb::Slice(key, KEY_SIZE);

	/*get the mutex lock DBLock_*/
	pthread_mutex_lock(&DBLock_);

	/*enquire the key in the database*/	
	fileStat = db_->Get(readOptions_, *fileKeySlice, &valueString);

	/*if such an inode for fullFileName exists*/
	if (fileStat.ok()) {	
		valueSize = valueString.size() + inodeFileEntrySize_;
		value = (char *) malloc(valueSize);

		/*copy the head from valueString to value*/		
		valueOffset = 0;	
		memcpy(value + valueOffset, valueString.data() + valueOffset, inodeIndexValueHeadSize_);	
		pInodeIndexValueHead = (inodeIndexValueHead_t *) (value + valueOffset);
		valueOffset += inodeIndexValueHeadSize_;

		/*copy the short name from valueString to value*/		
		memcpy(value + valueOffset, valueString.data() + valueOffset, pInodeIndexValueHead->shortNameSize);	
		valueOffset += pInodeIndexValueHead->shortNameSize;	

		/*add a new entry that contains the recipe file information for the newest version of the file in the front*/
		pInodeFileEntry = (inodeFileEntry_t *) (value + valueOffset);
		strcpy(pInodeFileEntry->recipeFileName, targetBufferNode->recipeFileName);
		pInodeFileEntry->recipeFileOffset = targetBufferNode->recipeFileBufferCurrLen;

		/*update the inode head (note: numOfChildren records the number of versions in this case)*/
		pInodeIndexValueHead->numOfChildren++;

		/*copy all other entries for older versions of the file from valueString to value*/	
		memcpy(value + valueOffset + inodeFileEntrySize_, valueString.data() + valueOffset, 
				valueString.size() - valueOffset);		

		/*clear the write batch batch_*/
		batch_.Clear();		

		/*update the key-value entry in a batch manner*/
		batch_.Delete(*fileKeySlice);
		valueSlice = new leveldb::Slice(value, valueSize);
		batch_.Put(*fileKeySlice, *valueSlice);

		/*execute all batched database update ops*/
		leveldb::Status writeStat = db_->Write(writeOptions_, &batch_);
		if (writeStat.ok() == false) {
			fprintf(stderr, "Error: fail to perform batched writes!\n");
			fprintf(stderr, "Status: %s \n", writeStat.ToString().c_str());

			/*release the mutex lock DBLock_*/
			pthread_mutex_unlock(&DBLock_);

			return 0;
		}

		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		delete valueSlice;
		free(value);
	}

	/*if such an inode for fullFileName does not exist*/
	if (fileStat.IsNotFound()) {	
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		/*1. first add a new key-value entry for fullFileName in the inode index*/
		valueSize = inodeIndexValueHeadSize_ + (shortName.size() + 1) + inodeFileEntrySize_;
		value = (char *) malloc(valueSize);

		/*generate the value*/

		/*- set the head*/
		valueOffset = 0;
		pInodeIndexValueHead = (inodeIndexValueHead_t *) (value + valueOffset);
		pInodeIndexValueHead->userID = userID;
		pInodeIndexValueHead->shortNameSize = shortName.size() + 1; /*size() do not consider the null-terminator*/
		pInodeIndexValueHead->inodeType = FILE_TYPE;
		pInodeIndexValueHead->numOfChildren = 1;
		valueOffset += inodeIndexValueHeadSize_;

		/*- set the short name*/
		strcpy(value + valueOffset, shortName.c_str());
		valueOffset += pInodeIndexValueHead->shortNameSize;

		/*- set the recipe file information*/
		pInodeFileEntry = (inodeFileEntry_t *) (value + valueOffset);
		strcpy(pInodeFileEntry->recipeFileName, targetBufferNode->recipeFileName);
		pInodeFileEntry->recipeFileOffset = targetBufferNode->recipeFileBufferCurrLen;
		valueOffset += inodeFileEntrySize_;

		valueSlice = new leveldb::Slice(value, valueSize);

		/*get the mutex lock DBLock_*/
		pthread_mutex_lock(&DBLock_);

		/*store the key-value entry into the inode index*/
		db_->Put(writeOptions_, *fileKeySlice, *valueSlice);

		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		delete valueSlice;
		free(value);

		/*2. update the inode of each dir in the path of fullFileName from bottom to top (the root '/')*/

		while (!dirName.empty()) {
			/*copy the previous fingerprint from currFP into preFP*/
			memcpy(preFP, currFP, FP_SIZE);

			/*generate the key of dirName*/
			fileName2InodeFP_(dirName, userID, currFP, cryptoObj);
			inodeFP2IndexKey_(currFP, key); 	
			dirKeySlice = new leveldb::Slice(key, KEY_SIZE);

			/*get the mutex lock DBLock_*/
			pthread_mutex_lock(&DBLock_);

			/*enquire the key in the database*/
			dirStat = db_->Get(readOptions_, *dirKeySlice, &valueString);

			/*if such an inode for dirName exists*/
			if (dirStat.ok()) {	 
				/*read the inode head*/
				valueOffset = 0;	
				pInodeIndexValueHead = (inodeIndexValueHead_t *) (valueString.data() + valueOffset);
				valueOffset += inodeIndexValueHeadSize_;

				/*skip the name*/
				valueOffset += pInodeIndexValueHead->shortNameSize;

				/*check if dirName has a path to fullFileName*/
				inPathFlag = 0;
				for (i = 0; (i < pInodeIndexValueHead->numOfChildren) && (inPathFlag == 0); i++) {
					pInodeDirEntry = (inodeDirEntry_t *) (valueString.data() + valueOffset);				
					valueOffset += inodeDirEntrySize_;

					if (memcmp(pInodeDirEntry->inodeFP, preFP, FP_SIZE) == 0) {
						inPathFlag = 1;
					}
				}

				/*if dirName does not have a path to fullFileName, add an entry for fullFileName*/
				if (inPathFlag != 1) {
					valueSize = valueString.size() + inodeDirEntrySize_;
					value = (char *) malloc(valueSize);

					/*copy the value from valueString to value*/
					memcpy(value, valueString.data(), valueString.size());

					/*add an entry for fullFileName*/
					valueOffset = valueString.size();	
					pInodeDirEntry = (inodeDirEntry_t *) (value + valueOffset);	
					memcpy(pInodeDirEntry->inodeFP, preFP, FP_SIZE); 	

					/*update the inode head*/
					valueOffset = 0;				
					pInodeIndexValueHead = (inodeIndexValueHead_t *) (value + valueOffset);
					pInodeIndexValueHead->numOfChildren++;

					/*clear the write batch batch_*/
					batch_.Clear();

					/*update the key-value entry in a batch manner*/
					batch_.Delete(*dirKeySlice);
					valueSlice = new leveldb::Slice(value, valueSize);
					batch_.Put(*dirKeySlice, *valueSlice);

					/*execute all batched database update ops*/
					leveldb::Status writeStat = db_->Write(writeOptions_, &batch_);
					if (writeStat.ok() == false) {
						fprintf(stderr, "Error: fail to perform batched writes!\n");
						fprintf(stderr, "Status: %s \n", writeStat.ToString().c_str());

						/*release the mutex lock DBLock_*/
						pthread_mutex_unlock(&DBLock_);

						return 0;
					}

					/*release the mutex lock DBLock_*/
					pthread_mutex_unlock(&DBLock_);

					delete valueSlice;
					free(value);
				}
				else {
					/*do nothing*/

					/*release the mutex lock DBLock_*/
					pthread_mutex_unlock(&DBLock_);
				}
			}

			/*if such an inode for dirName does not exist*/
			if (dirStat.IsNotFound()) {
				/*release the mutex lock DBLock_*/
				pthread_mutex_unlock(&DBLock_);

				if (dirName != "/") {
					/*since dirName ends with '/', we actually search the second last '/'*/
					currPos = dirName.rfind('/', dirName.size() - 2);
					/*for the dirName, shortName also ends with '/'*/
					shortName = dirName.substr(currPos + 1);					
				}
				else { /*we reach the root '/'*/
					shortName = dirName;
				}

				valueSize = inodeIndexValueHeadSize_ + (shortName.size() + 1) + inodeDirEntrySize_;
				value = (char *) malloc(valueSize);

				/*(a) generate the value*/

				/*- set the head*/
				valueOffset = 0;
				pInodeIndexValueHead = (inodeIndexValueHead_t *) (value + valueOffset);
				pInodeIndexValueHead->userID = userID;
				pInodeIndexValueHead->shortNameSize = shortName.size() + 1; /*size() do not consider the null-terminator*/
				pInodeIndexValueHead->inodeType = DIR_TYPE;
				pInodeIndexValueHead->numOfChildren = 1;
				valueOffset += inodeIndexValueHeadSize_;		

				/*- set the short name*/
				strcpy(value + valueOffset, shortName.c_str());
				valueOffset += pInodeIndexValueHead->shortNameSize;

				/*- set an entry for fullFileName*/
				pInodeDirEntry = (inodeDirEntry_t *) (value + valueOffset);	
				memcpy(pInodeDirEntry->inodeFP, preFP, FP_SIZE); 
				valueOffset += inodeDirEntrySize_;

				valueSlice = new leveldb::Slice(value, valueSize);

				/*get the mutex lock DBLock_*/
				pthread_mutex_lock(&DBLock_);

				/*(b) store the key-value entry into the inode index*/
				db_->Put(writeOptions_, *dirKeySlice, *valueSlice);	

				/*release the mutex lock DBLock_*/
				pthread_mutex_unlock(&DBLock_);

				delete valueSlice;
				free(value);		
			}

			if (dirStat.IsCorruption()) { 
				/*release the mutex lock DBLock_*/
				pthread_mutex_unlock(&DBLock_);

				fprintf(stderr, "Error: a corruption error occurs for the key '%s' in the database!\n", 
						dirKeySlice->ToString().c_str());

				delete dirKeySlice;

				return 0;
			}

			if (dirStat.IsIOError()) {
				/*release the mutex lock DBLock_*/
				pthread_mutex_unlock(&DBLock_);

				fprintf(stderr, "Error: an I/O error occurs for the key '%s' in the database!\n", 
						dirKeySlice->ToString().c_str());				

				delete dirKeySlice;

				return 0;
			}

			delete dirKeySlice;

			if (dirName != "/") {
				/*since dirName ends with '/', we actually search the second last '/'*/
				currPos = dirName.rfind('/', dirName.size() - 2);
				/*shorten dirName by one level*/
				dirName.resize(currPos + 1);				
			}
			else { /*we reach the root '/'*/
				dirName.clear();
			}
		}
	}

	if (fileStat.IsCorruption()) { 
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		fprintf(stderr, "Error: a corruption error occurs for the key '%s' in the database!\n", fileKeySlice->ToString().c_str());

		delete fileKeySlice;

		return 0;
	}

	if (fileStat.IsIOError()) {
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		fprintf(stderr, "Error: an I/O error occurs for the key '%s' in the database!\n", fileKeySlice->ToString().c_str());

		delete fileKeySlice;

		return 0;
	}	

	delete fileKeySlice;

	return 1;
}

/*
 * update the index for a share based on intra-user deduplication
 *
 * @param shareFP - the fingerprint of the share 
 * @param userID - the user id 
 * @param intraUserDupStat - a boolean value that indicates if the key is a duplicate <return>
 *
 * @return - a boolean value that indicates if the update op succeeds
 */
bool DedupCore::intraUserIndexUpdate_(char *shareFP, const int &userID, bool &intraUserDupStat) {
	char key[KEY_SIZE];
	char *value;
	leveldb::Slice *keySlice, *valueSlice;
	std::string valueString;
	int valueSize, valueOffset;
	shareIndexValueHead_t *pShareIndexValueHead;
	shareUserRefEntry_t *pShareUserRefEntry;

	shareFP2IndexKey_(shareFP, key);
	keySlice = new leveldb::Slice(key, KEY_SIZE); 

	/*get the mutex lock DBLock_*/
	pthread_mutex_lock(&DBLock_);

	leveldb::Status getStat = db_->Get(readOptions_, *keySlice, &valueString);

	if (getStat.ok()) { 
		valueOffset = 0;
		pShareIndexValueHead = (shareIndexValueHead_t *) (valueString.data() + valueOffset);
		valueOffset += shareIndexValueHeadSize_;

		/*check if the user owns the share corresponding to the key*/
		intraUserDupStat = 0;
		for (int i = 0; (i < pShareIndexValueHead->numOfUsers) && (intraUserDupStat == 0); i++) {
			pShareUserRefEntry = (shareUserRefEntry_t *) (valueString.data() + valueOffset);
			valueOffset += shareUserRefEntrySize_;			

			if (pShareUserRefEntry->userID == userID) {
				intraUserDupStat = 1;
			}
		}

		/*if the user owns the key, then update the user reference count*/
		if (intraUserDupStat == 1) {
			/*take a step back for updating the user reference count later*/
			valueOffset -= shareUserRefEntrySize_;

			valueSize = valueString.size();
			value = (char *) malloc(valueSize);

			/*copy the value from valueString to value*/
			memcpy(value, valueString.data(), valueString.size());

			/*update the user reference count*/	
			pShareUserRefEntry = (shareUserRefEntry_t *) (value + valueOffset);
			pShareUserRefEntry->refCnt++;

			/*clear the write batch batch_*/
			batch_.Clear();

			/*update the key-value entry in a batch manner*/
			batch_.Delete(*keySlice);
			valueSlice = new leveldb::Slice(value, valueSize);
			batch_.Put(*keySlice, *valueSlice);	

			/*execute all batched database update ops*/
			leveldb::Status writeStat = db_->Write(writeOptions_, &batch_);
			if (writeStat.ok() == false) {
				fprintf(stderr, "Error: fail to perform batched writes!\n");
				fprintf(stderr, "Status: %s \n", writeStat.ToString().c_str());

				/*release the mutex lock DBLock_*/
				pthread_mutex_unlock(&DBLock_);

				return 0;
			}

			/*release the mutex lock DBLock_*/
			pthread_mutex_unlock(&DBLock_);

			delete valueSlice;
			free(value);
		}
		else {
			/*do nothing*/

			/*release the mutex lock DBLock_*/
			pthread_mutex_unlock(&DBLock_);
		}
	}

	if (getStat.IsNotFound()) {
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		intraUserDupStat = 0;
	}	

	if (getStat.IsCorruption()) { 
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		fprintf(stderr, "Error: a corruption error occurs for the key '%s' in the database!\n", keySlice->ToString().c_str());

		delete keySlice;

		return 0;
	}		

	if (getStat.IsIOError()) { 
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		fprintf(stderr, "Error: an I/O error occurs for the key '%s' in the database!\n", keySlice->ToString().c_str());

		delete keySlice;

		return 0;
	}	

	delete keySlice;

	return 1;
}

/*
 * update the index for a share based on inter-user deduplication
 *
 * @param shareFP - the fingerprint of the share 
 * @param userID - the user id 
 * @param shareSize - the size of the corresponding share 
 * @param targetBufferNode - the corresponding buffer node 
 * @param shareDataBuffer - the share data buffer
 * @param shareDataBufferOffset - the offset of the share data buffer
 *
 * @return - a boolean value that indicates if the update op succeeds
 */
bool DedupCore::interUserIndexUpdate_(char *shareFP, const int &userID, const int &shareSize, 
		perUserBufferNode_t *targetBufferNode, unsigned char *shareDataBuffer, const int &shareDataBufferOffset) {	
	char key[KEY_SIZE];
	char *value;
	leveldb::Slice *keySlice, *valueSlice;
	std::string valueString;
	int valueSize, valueOffset;
	shareIndexValueHead_t *pShareIndexValueHead;
	shareUserRefEntry_t *pShareUserRefEntry;
	std::string shareContainerName;

	shareFP2IndexKey_(shareFP, key);
	keySlice = new leveldb::Slice(key, KEY_SIZE);

	/*get the mutex lock DBLock_*/
	pthread_mutex_lock(&DBLock_);

	leveldb::Status getStat = db_->Get(readOptions_, *keySlice, &valueString);

	if (getStat.ok()) { 		
		valueOffset = 0;
		pShareIndexValueHead = (shareIndexValueHead_t *) (valueString.data() + valueOffset);
		valueOffset += shareIndexValueHeadSize_;

		/*note: we here still check if the user owns the share, in consideration of the special case 
		  where the received package of shares contain repeated shares. The underlying reason is 
		  that in the first stage of deduplication, we do not check if there is any repeated share contained 
		  in the coming package of shares*/

		/*check if the user owns the share corresponding to the key*/
		int ownerStat = 0;
		for (int i = 0; (i < pShareIndexValueHead->numOfUsers) && (ownerStat == 0); i++) {
			pShareUserRefEntry = (shareUserRefEntry_t *) (valueString.data() + valueOffset);
			valueOffset += shareUserRefEntrySize_;			

			if (pShareUserRefEntry->userID == userID) {
				ownerStat = 1;
			}
		}

		/*if the user owns the key, then update the user reference count*/
		if (ownerStat == 1) { 
			/*take a step back for updating the user reference count later*/
			valueOffset -= shareUserRefEntrySize_;

			valueSize = valueString.size();
			value = (char *) malloc(valueSize);

			/*copy the value from valueString to value*/
			memcpy(value, valueString.data(), valueString.size());

			/*update the user reference count*/	
			pShareUserRefEntry = (shareUserRefEntry_t *) (value + valueOffset);
			pShareUserRefEntry->refCnt++;

			/*clear the write batch batch_*/
			batch_.Clear();

			/*update the key-value entry in a batch manner*/
			batch_.Delete(*keySlice);
			valueSlice = new leveldb::Slice(value, valueSize);
			batch_.Put(*keySlice, *valueSlice);

			/*execute all batched database update ops*/
			leveldb::Status writeStat = db_->Write(writeOptions_, &batch_);
			if (writeStat.ok() == false) {
				fprintf(stderr, "Error: fail to perform batched writes!\n");
				fprintf(stderr, "Status: %s \n", writeStat.ToString().c_str());

				/*release the mutex lock DBLock_*/
				pthread_mutex_unlock(&DBLock_);

				return 0;
			}

			/*release the mutex lock DBLock_*/
			pthread_mutex_unlock(&DBLock_);

			delete valueSlice;
			free(value);
		}
		/*if the user does not own the key, then append a reference entry for this user*/
		else {
			valueSize = valueString.size() + shareUserRefEntrySize_;
			value = (char *) malloc(valueSize);

			/*copy the value from valueString to value*/
			memcpy(value, valueString.data(), valueString.size());

			/*append a reference entry for this user*/
			valueOffset = valueString.size();
			pShareUserRefEntry = (shareUserRefEntry_t *) (value + valueOffset);
			pShareUserRefEntry->userID = userID;
			pShareUserRefEntry->refCnt = 1;

			/*update the share index value head*/
			valueOffset = 0;		
			pShareIndexValueHead = (shareIndexValueHead_t *) (value + valueOffset);
			pShareIndexValueHead->numOfUsers++;

			/*clear the write batch batch_*/
			batch_.Clear();

			/*update the key-value entry in a batch manner*/
			batch_.Delete(*keySlice);
			valueSlice = new leveldb::Slice(value, valueSize);
			batch_.Put(*keySlice, *valueSlice);	

			/*execute all batched database update ops*/
			leveldb::Status writeStat = db_->Write(writeOptions_, &batch_);
			if (writeStat.ok() == false) {
				fprintf(stderr, "Error: fail to perform batched writes!\n");
				fprintf(stderr, "Status: %s \n", writeStat.ToString().c_str());

				/*release the mutex lock DBLock_*/
				pthread_mutex_unlock(&DBLock_);

				return 0;
			}

			/*release the mutex lock DBLock_*/
			pthread_mutex_unlock(&DBLock_);

			delete valueSlice;
			free(value);	
		}
	}

	if (getStat.IsNotFound()) {
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		/*1. if there is no enough space in the share container buffer, first store the data of the buffer into the disk*/
		if (targetBufferNode->shareContainerBufferCurrLen + shareSize > CONTAINER_BUFFER_SIZE) {
			if (!storeShareContainer_(targetBufferNode, shareContainerName)) {
				fprintf(stderr, "Error: fail to store the data of the share container buffer into the disk!\n");
				return 0;
			}

			if (containerStorerObj_ != NULL) {
				containerStorerObj_->addNewFile(shareContainerName);
			}
		}

		/*2. add a new key-value entry for the share in the share index*/

		valueSize = shareIndexValueHeadSize_ + shareUserRefEntrySize_;
		value = (char *) malloc(valueSize);

		/*(a) generate the value*/

		/*- set the head*/
		valueOffset = 0;		
		pShareIndexValueHead = (shareIndexValueHead_t *) (value + valueOffset);	
		strcpy(pShareIndexValueHead->shareContainerName, targetBufferNode->shareContainerName);
		pShareIndexValueHead->shareContainerOffset = targetBufferNode->shareContainerBufferCurrLen;
		pShareIndexValueHead->shareSize = shareSize;
		pShareIndexValueHead->numOfUsers = 1;
		valueOffset += shareIndexValueHeadSize_;

		/*- set the user ref entry*/
		pShareUserRefEntry = (shareUserRefEntry_t *) (value + valueOffset);
		pShareUserRefEntry->userID = userID;
		pShareUserRefEntry->refCnt = 1;
		valueOffset += shareUserRefEntrySize_;

		valueSlice = new leveldb::Slice(value, valueSize);

		/*get the mutex lock DBLock_*/
		pthread_mutex_lock(&DBLock_);

		/*(b) store the key-value entry into the share index*/
		db_->Put(writeOptions_, *keySlice, *valueSlice);

		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		delete valueSlice;
		free(value);

		/*3. copy the share from shareDataBuffer into the buffer*/
		memcpy(targetBufferNode->shareContainerBuffer + targetBufferNode->shareContainerBufferCurrLen, 
				shareDataBuffer + shareDataBufferOffset, shareSize);
		targetBufferNode->shareContainerBufferCurrLen += shareSize;
	}	

	if (getStat.IsCorruption()) { 
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		fprintf(stderr, "Error: a corruption error occurs for the key '%s' in the database!\n", keySlice->ToString().c_str());

		delete keySlice;

		return 0;
	}		

	if (getStat.IsIOError()) { 
		/*release the mutex lock DBLock_*/
		pthread_mutex_unlock(&DBLock_);

		fprintf(stderr, "Error: an I/O error occurs for the key '%s' in the database!\n", keySlice->ToString().c_str());

		delete keySlice;

		return 0;
	}	

	delete keySlice;

	return 1;
}

/*
 * store the data of the recipe file buffer into a new recipe file
 *
 * @param targetBufferNode - the corresponding buffer node 
 * @param recipeFileName - the recipe file name <return>
 *
 * @return - a boolean value that indicates if the store op succeeds
 */
bool DedupCore::storeNewRecipeFile_(perUserBufferNode_t *targetBufferNode, std::string &recipeFileName) {
	recipeFileName = targetBufferNode->recipeFileName;	

	if (!addPrefixDir_(recipeFileDirName_, recipeFileName)) {
		fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", recipeFileDirName_.c_str(), recipeFileName.c_str());
		return 0;	
	}

	/*create a reciple file for writing recipes*/
	FILE *fp = fopen(recipeFileName.c_str(), "wb");		
	if (fp == NULL) {
		fprintf(stderr, "Error: fail to open the file '%s' for writing recipes!\n", recipeFileName.c_str());
		return 0;	
	}	

	/*write the data of the recipe file buffer into a recipe file*/
	if (fwrite(targetBufferNode->recipeFileBuffer, targetBufferNode->recipeFileBufferCurrLen, 1, fp) != 1){
		fprintf(stderr, "Error: fail to write recipes into the file '%s'!\n", recipeFileName.c_str());

		fclose(fp);
		return 0;	
	}	

	/*renew the recipeFileName of the buffer*/

	/*get the mutex lock globalRecipeFileNameLock_*/
	pthread_mutex_lock(&globalRecipeFileNameLock_);

	globalRecipeFileName_.copy(targetBufferNode->recipeFileName, INTERNAL_FILE_NAME_SIZE - 1, 0);
	targetBufferNode->recipeFileName[INTERNAL_FILE_NAME_SIZE - 1] = '\0';
	incrGlobalFileName_(globalRecipeFileName_, recipeFileNameValidLen_);		

	/*release the mutex lock globalRecipeFileNameLock_*/
	pthread_mutex_unlock(&globalRecipeFileNameLock_);

	/*renew the two positions of the buffer*/
	targetBufferNode->recipeFileBufferCurrLen = 0;
	targetBufferNode->lastRecipeHeadPos = 0;

	fclose(fp);
	return 1;
}

/*
 * find an old recipe file for appending the data of the recipe file buffer
 *
 * @param targetBufferNode - the corresponding buffer node 
 * @param recipeFileName - the name of the found old recipe file <return>
 * @param recipeFileOffset - the offset of the found old recipe file <return>
 *
 * @return - a boolean value that indicates if the find op succeeds
 */
bool DedupCore::findOldRecipeFile_(perUserBufferNode_t *targetBufferNode, std::string &recipeFileName, 
		int &recipeFileOffset) {
	char key[KEY_SIZE];
	leveldb::Slice *keySlice;
	std::string valueString;
	int valueOffset;
	inodeIndexValueHead_t *pInodeIndexValueHead;
	inodeFileEntry_t *pInodeFileEntry;

	/*generate the key*/
	inodeFP2IndexKey_(targetBufferNode->lastInodeFP, key);

	/*enquire the key in the database*/
	keySlice = new leveldb::Slice(key, KEY_SIZE);

	/*get the mutex lock DBLock_*/
	pthread_mutex_lock(&DBLock_);

	leveldb::Status getStat = db_->Get(readOptions_, *keySlice, &valueString);

	/*release the mutex lock DBLock_*/
	pthread_mutex_unlock(&DBLock_);

	/*if such an inode exists*/
	if (getStat.ok()) {
		/*read the inode head*/
		valueOffset = 0;	
		pInodeIndexValueHead = (inodeIndexValueHead_t *) (valueString.data() + valueOffset);
		valueOffset += inodeIndexValueHeadSize_;

		/*skip the name*/
		valueOffset += pInodeIndexValueHead->shortNameSize;

		/*read the inode file entry*/
		pInodeFileEntry = (inodeFileEntry_t *) (valueString.data() + valueOffset);
		valueOffset += inodeFileEntrySize_;

		/*get the recipe file info*/
		recipeFileName.assign(pInodeFileEntry->recipeFileName);
		recipeFileOffset = pInodeFileEntry->recipeFileOffset;		
	}

	/*if such an inode does not exist*/
	if (getStat.IsNotFound()) {	
		fprintf(stderr, "Error: cannot find an inode for the key '%s' in the database!\n", keySlice->ToString().c_str());		

		delete keySlice;

		return 0;		
	}

	if (getStat.IsCorruption()) { 
		fprintf(stderr, "Error: a corruption error occurs for the key '%s' in the database!\n", keySlice->ToString().c_str());

		delete keySlice;

		return 0;
	}

	if (getStat.IsIOError()) {
		fprintf(stderr, "Error: an I/O error occurs for the key '%s' in the database!\n", keySlice->ToString().c_str());

		delete keySlice;

		return 0;
	}

	delete keySlice;

	return 1;
}

/*
 * append the data of the recipe file buffer to an old recipe file
 *
 * @param targetBufferNode - the corresponding buffer node 
 * @param recipeFileName - the recipe file name <return>
 *
 * @return - a boolean value that indicates if the append op succeeds
 */
bool DedupCore::appendOldRecipeFile_(perUserBufferNode_t *targetBufferNode, std::string &recipeFileName) {
	int recipeFileOffset;
	fileRecipeHead_t *pFileRecipeHead;
	fileRecipeHead_t InsFileRecipeHead;

	/*find the old recipe file info in the database*/
	if (!findOldRecipeFile_(targetBufferNode, recipeFileName, recipeFileOffset)) {	
		fprintf(stderr, "Error: fail to find the old recipe file info in the database!\n");
		return 0;		
	}	

	if (!addPrefixDir_(recipeFileDirName_, recipeFileName)) {
		fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", recipeFileDirName_.c_str(), recipeFileName.c_str());
		return 0;	
	}

	/*open the recipe file for appending recipes*/
	FILE *fp = fopen(recipeFileName.c_str(), "rb+");		
	if (fp == NULL) {
		fprintf(stderr, "Error: fail to open the file '%s' for appending recipes!\n", recipeFileName.c_str());
		return 0;	
	}	

	long fileSize = getFileSize_(fp);
	if (fileSize == 0) {
		fprintf(stderr, "Error: the previous recipe file '%s' does not exit (or is empty)!\n", recipeFileName.c_str());

		fclose(fp);
		return 0;	
	}

	/*read the file recipe head from the recipe file*/
	fseek(fp, recipeFileOffset, SEEK_SET);
	if (fread(&InsFileRecipeHead, fileRecipeHeadSize_, 1, fp) != 1){
		fprintf(stderr, "Error: fail to read the file recipe head from the file '%s'!\n", recipeFileName.c_str());

		fclose(fp);
		return 0;	
	}	

	/*append file recipe entries to the recipe file*/
	fseek(fp, recipeFileOffset + fileRecipeHeadSize_ + fileRecipeEntrySize_ * (InsFileRecipeHead.numOfShares), SEEK_SET);
	if (fwrite(targetBufferNode->recipeFileBuffer + fileRecipeHeadSize_, 
				targetBufferNode->recipeFileBufferCurrLen - fileRecipeHeadSize_, 1, fp) != 1){
		fprintf(stderr, "Error: fail to append recipe entries to the file '%s'!\n", recipeFileName.c_str());

		fclose(fp);
		return 0;	
	}	

	/*update the file recipe head into the recipe file*/
	pFileRecipeHead = (fileRecipeHead_t *) (targetBufferNode->recipeFileBuffer);
	InsFileRecipeHead.numOfShares += pFileRecipeHead->numOfShares;
	fseek(fp, recipeFileOffset, SEEK_SET);
	if (fwrite(&InsFileRecipeHead, fileRecipeHeadSize_, 1, fp) != 1){
		fprintf(stderr, "Error: fail to update the file recipe head into the file '%s'!\n", recipeFileName.c_str());

		fclose(fp);
		return 0;	
	}	

	/*renew the two positions of the buffer*/
	targetBufferNode->recipeFileBufferCurrLen = 0;
	targetBufferNode->lastRecipeHeadPos = 0;

	fclose(fp);
	return 1;
}

/*
 * store the data of the share container buffer into a container
 *
 * @param targetBufferNode - the corresponding buffer node 
 * @param shareContainerName - the share container name <return>
 *
 * @return - a boolean value that indicates if the store op succeeds
 */
bool DedupCore::storeShareContainer_(perUserBufferNode_t *targetBufferNode, std::string &shareContainerName) {
	shareContainerName = targetBufferNode->shareContainerName;

	if (!addPrefixDir_(shareContainerDirName_, shareContainerName)) {
		fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", shareContainerDirName_.c_str(), shareContainerName.c_str());
		return 0;	
	}	

	/*create a container file for writing shares*/
	FILE *fp = fopen(shareContainerName.c_str(), "wb");		
	if (fp == NULL) {
		fprintf(stderr, "Error: fail to open the file '%s' for writing shares!\n", shareContainerName.c_str());
		return 0;	
	}	

	/*write the data of the share container buffer into a container file*/
	if (fwrite(targetBufferNode->shareContainerBuffer, targetBufferNode->shareContainerBufferCurrLen, 1, fp) != 1){
		fprintf(stderr, "Error: fail to write shares into the file '%s'!\n", shareContainerName.c_str());

		fclose(fp);
		return 0;	
	}	

	/*renew the share container buffer*/

	/*get the mutex lock globalShareContainerNameLock_*/
	pthread_mutex_lock(&globalShareContainerNameLock_);

	globalShareContainerName_.copy(targetBufferNode->shareContainerName, INTERNAL_FILE_NAME_SIZE-1, 0);
	targetBufferNode->shareContainerName[INTERNAL_FILE_NAME_SIZE - 1] = '\0';
	incrGlobalFileName_(globalShareContainerName_, shareContainerNameValidLen_);

	/*release the mutex lock globalShareContainerNameLock_*/
	pthread_mutex_unlock(&globalShareContainerNameLock_);

	targetBufferNode->shareContainerBufferCurrLen = 0;

	fclose(fp);
	return 1;
}

/*
 * read the recipe file from the buffer node link
 *
 * @param userID - the user id
 * @param recipeFileName - the name of the recipe file
 * @param recipeFileBuffer - the buffer for storing the recipe file <return>
 *
 * @return - a boolean value that indicates if the read op succeeds
 */
bool DedupCore::readRecipeFileFromBuffer_(const int &userID, char *recipeFileName, 
		unsigned char *recipeFileBuffer) {	
	perUserBufferNode_t *targetBufferNode;

	/*get the mutex lock bufferLock_*/
	pthread_mutex_lock(&bufferLock_);

	/*find the buffer node for this user*/
	targetBufferNode = headBufferNode_;
	while ((targetBufferNode != NULL) && (targetBufferNode->userID != userID)) {
		targetBufferNode = targetBufferNode->next;
	}

	/*if find the buffer node, then check if the recipe file is stored in the buffer*/
	if((targetBufferNode != NULL) && (strcmp(targetBufferNode->recipeFileName, recipeFileName) == 0)) {
		/*get the data of the recipe file buffer*/
		memcpy(recipeFileBuffer, targetBufferNode->recipeFileBuffer, 
				targetBufferNode->recipeFileBufferCurrLen);

		/*release the mutex lock bufferLock_*/
		pthread_mutex_unlock(&bufferLock_);

		return 1;
	}
	else {
		/*release the mutex lock bufferLock_*/
		pthread_mutex_unlock(&bufferLock_);

		return 0;
	}
}

/*
 * read the share container from the buffer node link
 *
 * @param shareContainerName - the name of the share container
 * @param shareContainerBuffer - the buffer for storing the share container <return>
 *
 * @return - a boolean value that indicates if the read op succeeds
 */
bool DedupCore::readShareContainerFromBuffer_(char *shareContainerName, 
		unsigned char *shareContainerBuffer) {	
	perUserBufferNode_t *targetBufferNode;

	pthread_mutex_lock(&bufferLock_);

	/*find the buffer node that contains the share container*/
	targetBufferNode = headBufferNode_;
	while ((targetBufferNode != NULL) && (strcmp(targetBufferNode->shareContainerName, shareContainerName) != 0)) {
		targetBufferNode = targetBufferNode->next;
	}

	/*if find the buffer node, then get the data of the share container buffer*/
	if(targetBufferNode != NULL) {
		memcpy(shareContainerBuffer, targetBufferNode->shareContainerBuffer, 
				targetBufferNode->shareContainerBufferCurrLen);

		pthread_mutex_unlock(&bufferLock_);

		return 1;
	}
	else {
		pthread_mutex_unlock(&bufferLock_);

		return 0;
	}
}

/*
 * perform the first-stage deduplication
 *
 * @param userID - the user id 
 * @param shareMDBuffer - the buffer that stores the share metadata
 * @param shareMDSize - the size of the share metadata buffer
 * @param intraUserDupStatList - a list that records the intra-user duplicate status of each share <return>
 * @param numOfShares - the total number of processed shares <return>
 * @param sentShareDataSize - the total size of share data that should be sent by the client <return>
 *
 * @return - a boolean value that indicates if the first-stage deduplication succeeds
 */
bool DedupCore::firstStageDedup(const int &userID, unsigned char *shareMDBuffer, const int &shareMDSize, 
		bool *intraUserDupStatList, int &numOfShares, int &sentShareDataSize) {
	fileShareMDHead_t *pFileShareMDHead;
	shareMDEntry_t *pShareMDEntry;
	int shareMDBufferOffset = 0;	
	int i;

	numOfShares = 0;
	sentShareDataSize = 0;

	while (shareMDBufferOffset < shareMDSize) {
		/*read the file share metadata head*/
		pFileShareMDHead = (fileShareMDHead_t *) (shareMDBuffer + shareMDBufferOffset);
		shareMDBufferOffset += fileShareMDHeadSize_;		

		/*skip the file name*/
		shareMDBufferOffset += pFileShareMDHead->fullNameSize;

		/*check each of the subsequent shares*/
		for (i = 0; i < pFileShareMDHead->numOfComingSecrets; i++) {
			/*read the share metadata entry*/
			pShareMDEntry = (shareMDEntry_t *) (shareMDBuffer + shareMDBufferOffset);
			shareMDBufferOffset += shareMDEntrySize_;

			/*check the intra-user duplicate status*/
			if (!intraUserIndexUpdate_(pShareMDEntry->shareFP, userID, intraUserDupStatList[numOfShares])) {
				fprintf(stderr, "Error: fail to update the share index for intra-user duplication in the database!\n");

				return 0;
			}
			if (intraUserDupStatList[numOfShares] == 0) {
				sentShareDataSize += pShareMDEntry->shareSize;
			}

			numOfShares++;					
		}		
	}

	return 1;
}


/*
 * perform the second-stage deduplication
 *
 * @param userID - the user id 
 * @param shareMDBuffer - the buffer that stores the share metadata
 * @param shareMDSize - the size of the share metadata buffer
 * @param intraUserDupStatList - a list that records the intra-user duplicate status of each share 
 * @param shareDataBuffer - the share data buffer
 * @param cryptoObj - the CryptoPrimitive instance for calculating hash fingerprint
 *
 * @return - a boolean value that indicates if the second-stage deduplication succeeds
 */
bool DedupCore::secondStageDedup(const int &userID, unsigned char *shareMDBuffer, const int &shareMDSize, 
		bool *intraUserDupStatList, unsigned char *shareDataBuffer, CryptoPrimitive *cryptoObj) {
	perUserBufferNode_t *targetBufferNode;
	fileShareMDHead_t *pFileShareMDHead;
	shareMDEntry_t *pShareMDEntry;
	fileRecipeHead_t *pFileRecipeHead;
	fileRecipeEntry_t *pFileRecipeEntry;
	std::string fullFileName;
	char shareFP[FP_SIZE];
	int shareMDBufferOffset = 0, shareDataBufferOffset = 0;	
	int recipeFileBufferAddedLen;
	std::string recipeFileName;
	int numOfShares = 0;
	int i;

	if (cryptoObj == NULL) {		
		fprintf(stderr, "Error: no CryptoPrimitive instance for calculating hash fingerprint!\n");					
		return 0;	
	}

	/*find the corresponding buffer node for the user*/
	targetBufferNode = NULL;
	findOrCreateBufferNode_(userID, targetBufferNode);	

	while (shareMDBufferOffset < shareMDSize) {
		/*1. read the file share metadata head and file name*/

		/*read the file share metadata head*/
		pFileShareMDHead = (fileShareMDHead_t *) (shareMDBuffer + shareMDBufferOffset);
		shareMDBufferOffset += fileShareMDHeadSize_;		

		/*read the file name with null-terminator*/
		fullFileName.assign((char *) (shareMDBuffer + shareMDBufferOffset), pFileShareMDHead->fullNameSize);
		shareMDBufferOffset += pFileShareMDHead->fullNameSize;
		/*format fullFileName before using it*/
		if (!formatFullFileName_(fullFileName)) {
			fprintf(stderr, "Error: encounter an invalid fullFileName!\n");

			return 0;
		}	

		/*2. make sure the recipe file buffer has enough space for storing the coming file recipe head and entries*/

		/*if this is a new file*/
		if (pFileShareMDHead->numOfPastSecrets == 0) {
			recipeFileBufferAddedLen = fileRecipeHeadSize_ + 
				fileRecipeEntrySize_ * (pFileShareMDHead->numOfComingSecrets);
		}
		/*if this is the remain of a previous file*/
		else {
			/*if the buffer is empty, we also need a file recipe head*/
			if (targetBufferNode->recipeFileBufferCurrLen == 0) {
				recipeFileBufferAddedLen = fileRecipeHeadSize_ + 
					fileRecipeEntrySize_ * (pFileShareMDHead->numOfComingSecrets);		
			}
			/*otherwise, the buffer should already store the file recipe head*/
			else {
				recipeFileBufferAddedLen = fileRecipeEntrySize_ * (pFileShareMDHead->numOfComingSecrets);
			}			
		}

		/*if there is no enough space in the recipe file buffer, store the data of the buffer into the disk*/
		if (targetBufferNode->recipeFileBufferCurrLen + recipeFileBufferAddedLen > RECIPE_BUFFER_SIZE) {
			/*read the last recipe head*/
			pFileRecipeHead = (fileRecipeHead_t *) (targetBufferNode->recipeFileBuffer + 
					targetBufferNode->lastRecipeHeadPos);

			/*if (a) the last recipe head is at the beginning of the buffer and 
			  (b) the first recipe entry has been stored in a previous recipe file*/
			if ((targetBufferNode->lastRecipeHeadPos == 0) && 
					(pFileShareMDHead->numOfPastSecrets > pFileRecipeHead->numOfShares)) {
				/*since pFileShareMDHead->numOfPastSecrets > 0, the coming shares are for the same fullFileName*/
				if (!appendOldRecipeFile_(targetBufferNode, recipeFileName)) {
					fprintf(stderr, "Error: fail to append the data of the recipe file buffer to a previous recipe file!\n");

					return 0;
				}
			}
			else {
				if (!storeNewRecipeFile_(targetBufferNode, recipeFileName)) {
					fprintf(stderr, "Error: fail to store the data of the recipe file buffer into a new recipe file!\n");

					return 0;
				}
			}

			/*if a new file starts, then the previous file has been finished*/
			if (pFileShareMDHead->numOfPastSecrets == 0) {
				if (recipeStorerObj_ != NULL) {
					recipeStorerObj_->addNewFile(recipeFileName);
				}
			}
		}

		/*3. store the file recipe head, or update a previous one in the recipe file buffer*/

		/*if this is a new file*/
		if (pFileShareMDHead->numOfPastSecrets == 0) { 				
			if (!addFileIntoInodeIndex_(fullFileName, userID, targetBufferNode, cryptoObj)) {
				fprintf(stderr, "Error: fail to add an inode for fullFileName '%s' with userID '%d' in the database!\n", 
						fullFileName.c_str(), userID);

				return 0;
			}			

			/*put the file recipe head into recipeFileBuffer*/
			pFileRecipeHead = (fileRecipeHead_t *) (targetBufferNode->recipeFileBuffer + 
					targetBufferNode->recipeFileBufferCurrLen);
			pFileRecipeHead->userID = userID;
			pFileRecipeHead->fileSize = pFileShareMDHead->fileSize;
			pFileRecipeHead->numOfShares = pFileShareMDHead->numOfComingSecrets;

			/*update the info of targetBufferNode*/
			targetBufferNode->lastRecipeHeadPos = targetBufferNode->recipeFileBufferCurrLen;	
			targetBufferNode->recipeFileBufferCurrLen += fileRecipeHeadSize_;			
			fileName2InodeFP_(fullFileName, userID, targetBufferNode->lastInodeFP, cryptoObj);				
		}
		/*if this is the remain of a previous file*/
		else {
			/*if the buffer is empty, we also need a file recipe head*/
			if (targetBufferNode->recipeFileBufferCurrLen == 0) {				
				/*put the file recipe head into recipeFileBuffer*/
				pFileRecipeHead = (fileRecipeHead_t *) (targetBufferNode->recipeFileBuffer + 
						targetBufferNode->recipeFileBufferCurrLen);
				pFileRecipeHead->userID = userID;
				pFileRecipeHead->fileSize = pFileShareMDHead->fileSize;
				pFileRecipeHead->numOfShares = pFileShareMDHead->numOfComingSecrets;

				/*update the info of targetBufferNode*/
				targetBufferNode->lastRecipeHeadPos = targetBufferNode->recipeFileBufferCurrLen;
				targetBufferNode->recipeFileBufferCurrLen += fileRecipeHeadSize_;
				fileName2InodeFP_(fullFileName, userID, targetBufferNode->lastInodeFP, cryptoObj);
			}
			/*otherwise, the buffer should already store the file recipe head*/
			else {				
				/*update the file recipe head in recipeFileBuffer*/
				pFileRecipeHead = (fileRecipeHead_t *) (targetBufferNode->recipeFileBuffer + 
						targetBufferNode->lastRecipeHeadPos);
				pFileRecipeHead->numOfShares += pFileShareMDHead->numOfComingSecrets;
			}			
		}

		/*4. store each file recipe entry in the recipe file buffer*/

		/*deal with each of the subsequent shares*/
		for (i = 0; i < pFileShareMDHead->numOfComingSecrets; i++) {
			/*read the share metadata entry*/
			pShareMDEntry = (shareMDEntry_t *) (shareMDBuffer + shareMDBufferOffset);
			shareMDBufferOffset += shareMDEntrySize_;

			/*if the share is not a duplicate in intra-user deduplication, further perform inter-user deduplication on it*/
			if (intraUserDupStatList[numOfShares] != 1) {
				/*generate a hash fingerprint from the share and check if the generated one is consistent with the received one*/
				cryptoObj->generateHash(shareDataBuffer + shareDataBufferOffset, pShareMDEntry->shareSize, 
						(unsigned char *) shareFP);
				if (memcmp(pShareMDEntry->shareFP, shareFP, FP_SIZE) != 0) {
					fprintf(stderr, "Error: the %d-th share and its fingerprint sent by userID '%d' are inconsistent!\n", i, userID);

					return 0;
				}

				if (!interUserIndexUpdate_(shareFP, userID, pShareMDEntry->shareSize, targetBufferNode, shareDataBuffer, 
							shareDataBufferOffset)) {
					fprintf(stderr, "Error: fail to update the share index for inter-user duplication in the database!\n");

					return 0;
				}

				shareDataBufferOffset += pShareMDEntry->shareSize;
			}

			/*put the file recipe entry into recipeFileBuffer*/
			pFileRecipeEntry = (fileRecipeEntry_t *) (targetBufferNode->recipeFileBuffer + 
					targetBufferNode->recipeFileBufferCurrLen);
			memcpy(pFileRecipeEntry->shareFP, pShareMDEntry->shareFP, FP_SIZE);
			pFileRecipeEntry->secretID = pShareMDEntry->secretID;
			pFileRecipeEntry->secretSize = pShareMDEntry->secretSize;

			/*update the info of targetBufferNode*/
			targetBufferNode->recipeFileBufferCurrLen += fileRecipeEntrySize_;

			numOfShares++;					
		}

		/*5. if a partial file ends here, append its recipe entries to a previous recipe file*/

		/*if the file ends here*/
		if (pFileShareMDHead->sizeOfPastSecrets + pFileShareMDHead->sizeOfComingSecrets == 
				pFileShareMDHead->fileSize) {
			/*if (a) the file recipe head is at the beginning of the buffer and 
			  (b) the first recipe entry has been stored in a previous recipe file*/
			if ((targetBufferNode->lastRecipeHeadPos == 0) && 
					(pFileShareMDHead->numOfPastSecrets > pFileRecipeHead->numOfShares)) {
				if (!appendOldRecipeFile_(targetBufferNode, recipeFileName)) {
					fprintf(stderr, "Error: fail to append the data of the recipe file buffer to a previous recipe file!\n");

					return 0;
				}

				if (recipeStorerObj_ != NULL) {
					recipeStorerObj_->addNewFile(recipeFileName);
				}
			}
			else {
				/*do nothing*/
			}
		}

	}

	return 1;
}

/*
 * clean up the buffer node for a user in the buffer node link
 *
 * @param userID - the user id
 *
 * @return - a boolean value that indicates if the clean-up op succeeds
 */
bool DedupCore::cleanupUserBufferNode(const int &userID) {	
	perUserBufferNode_t *targetBufferNode, *preBufferNode;

	/*get the mutex lock bufferLock_*/
	pthread_mutex_lock(&bufferLock_);

	/*find the buffer node for this user*/
	targetBufferNode = headBufferNode_;
	preBufferNode =NULL;
	while ((targetBufferNode != NULL) && (targetBufferNode->userID != userID)) {
		preBufferNode = targetBufferNode;
		targetBufferNode = targetBufferNode->next;
	}

	if (targetBufferNode != NULL) {
		/* flush the buffer node into the disk*/
		if (!flushBufferNodeIntoDisk_(targetBufferNode)) {
			fprintf(stderr, "Error: fail to flush the buffer node for userID '%d' into the disk!\n", targetBufferNode->userID);

			/*release the mutex lock bufferLock_*/
			pthread_mutex_unlock(&bufferLock_);

			return 0;
		}

		/*delete the buffer node from the link and then free it*/
		if (targetBufferNode == headBufferNode_) {
			headBufferNode_ = targetBufferNode->next;
			free(targetBufferNode);

			if (headBufferNode_ == tailBufferNode_) {
				/*the only buffer node is deleted*/
				tailBufferNode_ = NULL;
			}		
		}
		else {
			if (targetBufferNode == tailBufferNode_) {
				tailBufferNode_ = preBufferNode;
			}

			preBufferNode->next = targetBufferNode->next;
			free(targetBufferNode);
		}

		/*release the mutex lock bufferLock_*/
		pthread_mutex_unlock(&bufferLock_);

		return 1;
	}
	else {
		fprintf(stderr, "Error: cannot find the buffer node for userID '%d'!\n", targetBufferNode->userID);

		/*release the mutex lock bufferLock_*/
		pthread_mutex_unlock(&bufferLock_);

		return 0;
	}
}

/*
 * clean up the buffer nodes for all users in the buffer node link
 *
 * @return - a boolean value that indicates if the clean-up op succeeds
 */
bool DedupCore::cleanupAllBufferNodes() {	
	perUserBufferNode_t *currBufferNode, *nextBufferNode;

	/*free the file recipe pool*/
	if (headBufferNode_ != NULL) {
		/*get the mutex lock bufferLock_*/
		pthread_mutex_lock(&bufferLock_);

		currBufferNode = headBufferNode_;
		while (currBufferNode != NULL){			
			/* flush the buffer node into the disk*/
			if (!flushBufferNodeIntoDisk_(currBufferNode)) {
				fprintf(stderr, "Error: fail to flush the buffer node for userID '%d' into the disk!\n", 
						currBufferNode->userID);

				return 0;
			}

			nextBufferNode = currBufferNode->next;
			free(currBufferNode);			
			currBufferNode = nextBufferNode;
		}	

		/*release the mutex lock bufferLock_*/
		pthread_mutex_unlock(&bufferLock_);
	}

	/*ensure that all container files are committed to disk*/
	sync();

	return 1;
}

/*
 * restore a share file for a user and send it through the socket
 *
 * @param userID - the user id
 * @param fullFileName - the full name of the original file 
 * @param versionNumber - the version number (<=0) of the original file 
 * @param socketFD - the file descriptor of the sending socket
 * @param cryptoObj - the CryptoPrimitive instance for calculating hash fingerprint
 *
 * @return - a boolean value that indicates if the restore op succeeds
 */
bool DedupCore::restoreShareFile(const int &userID, const std::string &fullFileName, const int &versionNumber, 
		int socketFD, CryptoPrimitive *cryptoObj) {
	leveldb::Status inodeStat, shareStat;
	std::string formatedFullFileName;
	char FP[FP_SIZE];		
	char key[KEY_SIZE];
	leveldb::Slice *inodeKeySlice, *shareKeySlice;
	std::string valueString;
	int valueOffset;
	bool recipeFileIsInBuffer;
	unsigned char *recipeFileBuffer, *shareFileBuffer;
	int recipeFileBufferOffset, recipeFileBufferTailLen, shareFileBufferOffset;
	shareContainerCacheNode_t *shareContainerCache;
	int *shareContainerCacheIndex, numOfCachedShareContainers;
	FILE *recipeFilePointer, *containerFilePointer;
	std::string fullRecipeFileName, fullShareContainerName;
	int numOfShares;
	inodeIndexValueHead_t *pInodeIndexValueHead;
	inodeFileEntry_t *pInodeFileEntry;
	shareIndexValueHead_t *pShareIndexValueHead;
	fileRecipeHead_t *pFileRecipeHead;
	fileRecipeEntry_t *pFileRecipeEntry;
	shareFileHead_t *pShareFileHead;
	shareEntry_t *pShareEntry;	
	ssize_t sentSize;	
	uint32_t indicator;
	uint32_t sentDataSize;
	int i, j, k;

	if (cryptoObj == NULL) {		
		fprintf(stderr, "Error: no CryptoPrimitive instance for calculating hash fingerprint!\n");					
		return 0;	
	}

	/*format the full file name*/
	formatedFullFileName = fullFileName;		
	if (!formatFullFileName_(formatedFullFileName)) {
		fprintf(stderr, "Error: encounter an invalid fullFileName!\n");

		return 0;
	}	

	/*generate the key*/
	fileName2InodeFP_(formatedFullFileName, userID, FP, cryptoObj);	
	inodeFP2IndexKey_(FP, key);
	inodeKeySlice = new leveldb::Slice(key, KEY_SIZE);

	/*get the mutex lock DBLock_*/
	pthread_mutex_lock(&DBLock_);

	/*enquire the key in the database*/
	inodeStat = db_->Get(readOptions_, *inodeKeySlice, &valueString);

	/*release the mutex lock DBLock_*/
	pthread_mutex_unlock(&DBLock_);

	/*if such an inode for fullFileName exists*/
	if (inodeStat.ok()) {	
		/*enlarge the share file buffer size with a message head (indicator, sentDataSize)*/
		int sentMsgHeadSize = sizeof(uint32_t) * 2;
		int sentShareFileBufferSize = sentMsgHeadSize + SHARE_FILE_BUFFER_SIZE;

		/*allocate some caches and buffers for accelerating file restoring speed*/
		recipeFileBuffer = (unsigned char *) malloc(sizeof(unsigned char) * RECIPE_BUFFER_SIZE);
		shareFileBuffer = (unsigned char *) malloc(sentShareFileBufferSize);
		shareContainerCache = (shareContainerCacheNode_t *) malloc(sizeof(shareContainerCacheNode_t) * NUM_OF_CACHED_CONTAINERS);
		shareContainerCacheIndex = (int *) malloc(sizeof(int) * NUM_OF_CACHED_CONTAINERS); 
		numOfCachedShareContainers = 0;

		/*read the inode head*/
		valueOffset = 0;	
		pInodeIndexValueHead = (inodeIndexValueHead_t *) (valueString.data() + valueOffset);
		valueOffset += inodeIndexValueHeadSize_;

		/*check the validity of the version number*/
		if ((-versionNumber) >= pInodeIndexValueHead->numOfChildren) {
			fprintf(stderr, "Error: no such an old version exists for the version number %d!\n", versionNumber);

			free(recipeFileBuffer);
			free(shareFileBuffer);
			free(shareContainerCache);
			free(shareContainerCacheIndex); 

			delete inodeKeySlice;

			return 0;	
		}	

		/*skip the name*/
		valueOffset += pInodeIndexValueHead->shortNameSize;

		/*read the inode file entry*/		
		valueOffset += (inodeFileEntrySize_ * (-versionNumber));
		pInodeFileEntry = (inodeFileEntry_t *) (valueString.data() + valueOffset);
		valueOffset += inodeFileEntrySize_;

		/*first read recipe file from the buffer; if it is not in the buffer, then read it from the disk*/
		if (!(recipeFileIsInBuffer = readRecipeFileFromBuffer_(userID, pInodeFileEntry->recipeFileName, recipeFileBuffer))) {
			/*generate the full recipe file name*/
			fullRecipeFileName = pInodeFileEntry->recipeFileName;
			if (!addPrefixDir_(recipeFileDirName_, fullRecipeFileName)) {
				fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", recipeFileDirName_.c_str(), 
						fullRecipeFileName.c_str());

				free(recipeFileBuffer);
				free(shareFileBuffer);
				free(shareContainerCache);
				free(shareContainerCacheIndex); 

				delete inodeKeySlice;

				return 0;	
			}

			/*open the recipe file for reading recipes*/
			if (recipeStorerObj_ != NULL) {
				recipeStorerObj_->openOldFile(fullRecipeFileName, recipeFilePointer);
			}
			else {
				recipeFilePointer = fopen(fullRecipeFileName.c_str(), "rb");
			}
			if (recipeFilePointer == NULL) {
				fprintf(stderr, "Error: fail to open the file '%s' for reading recipes!\n", fullRecipeFileName.c_str());

				free(recipeFileBuffer);
				free(shareFileBuffer);
				free(shareContainerCache);
				free(shareContainerCacheIndex); 

				delete inodeKeySlice;

				return 0;	
			}

			/*read the first RECIPE_BUFFER_SIZE bytes of the recipe file and store them into recipeFileBuffer*/
			fseek(recipeFilePointer, 0, SEEK_SET);
			if (fread(recipeFileBuffer, 1, RECIPE_BUFFER_SIZE, recipeFilePointer) == 0){
				fprintf(stderr, "Error: fail to read the recipe file '%s'!\n", fullRecipeFileName.c_str());

				fclose(recipeFilePointer);

				free(recipeFileBuffer);
				free(shareFileBuffer);
				free(shareContainerCache);
				free(shareContainerCacheIndex); 

				delete inodeKeySlice;

				return 0;	
			}
		}

		/*read the file recipe head*/
		recipeFileBufferOffset = pInodeFileEntry->recipeFileOffset;
		pFileRecipeHead = (fileRecipeHead_t *) (recipeFileBuffer + recipeFileBufferOffset);
		recipeFileBufferOffset += fileRecipeHeadSize_;

		/*generate and store the share file head into shareFileBuffer*/
		shareFileBufferOffset = sentMsgHeadSize;
		pShareFileHead = (shareFileHead_t *) (shareFileBuffer + shareFileBufferOffset);
		pShareFileHead->fileSize = pFileRecipeHead->fileSize;
		pShareFileHead->numOfShares = pFileRecipeHead->numOfShares;
		shareFileBufferOffset += shareFileHeadSize_;

		/*restore each share*/
		numOfShares = pShareFileHead->numOfShares;
		for (i = 0; i < numOfShares; i++) {
			/*check if recipeFileBuffer holds a complete file recipe entry*/
			if (recipeFileBufferOffset + fileRecipeEntrySize_ > RECIPE_BUFFER_SIZE) {
				if (recipeFileIsInBuffer) {
					fprintf(stderr, "Error: encounter incomplete file recipe in buffer!\n");

					free(recipeFileBuffer);
					free(shareFileBuffer);
					free(shareContainerCache);
					free(shareContainerCacheIndex); 

					delete inodeKeySlice;

					return 0;	
				}
				else {
					/*move the rest tail to the beginning*/
					recipeFileBufferTailLen = RECIPE_BUFFER_SIZE - recipeFileBufferOffset;
					memcpy(recipeFileBuffer, recipeFileBuffer + recipeFileBufferOffset, recipeFileBufferTailLen);

					/*read more data from the recipe file to fill in recipeFileBuffer*/
					if (fread(recipeFileBuffer + recipeFileBufferTailLen, 1, recipeFileBufferOffset, recipeFilePointer) == 0){
						fprintf(stderr, "Error: fail to read the recipe file '%s'!\n", fullRecipeFileName.c_str());

						fclose(recipeFilePointer);

						free(recipeFileBuffer);
						free(shareFileBuffer);
						free(shareContainerCache);
						free(shareContainerCacheIndex); 

						delete inodeKeySlice;

						return 0;	
					}

					/*reset recipeFileBufferOffset*/
					recipeFileBufferOffset = 0;
				}
			}

			/*read the file recipe entry*/
			pFileRecipeEntry = (fileRecipeEntry_t *) (recipeFileBuffer + recipeFileBufferOffset);
			recipeFileBufferOffset += fileRecipeEntrySize_;	

			/*generate the key for the corresponding share*/
			shareFP2IndexKey_(pFileRecipeEntry->shareFP, key);
			shareKeySlice = new leveldb::Slice(key, KEY_SIZE);

			/*get the mutex lock DBLock_*/
			pthread_mutex_lock(&DBLock_);	

			/*enquire the key in the database*/
			shareStat = db_->Get(readOptions_, *shareKeySlice, &valueString);

			/*release the mutex lock DBLock_*/
			pthread_mutex_unlock(&DBLock_);

			/*if such a share exists*/
			if (shareStat.ok()) {
				/*read the head of the share index value*/				
				valueOffset = 0;
				pShareIndexValueHead = (shareIndexValueHead_t *) (valueString.data() + valueOffset);
				valueOffset += shareIndexValueHeadSize_;

				/*check if the share container has been cached in shareContainerCache*/
				j = 0;
				while ((j < numOfCachedShareContainers) && (strcmp(pShareIndexValueHead->shareContainerName, 
								shareContainerCache[shareContainerCacheIndex[j]].shareContainerName) != 0)) {
					j++;
				}

				/*if the share container is cached in shareContainerCache*/
				if (j < numOfCachedShareContainers) {
					/*move the found share container to the beginning of the cache*/
					if (j > 0) {
						int tmp = shareContainerCacheIndex[j];
						for (k = j; k > 0; k--) {
							shareContainerCacheIndex[k] = shareContainerCacheIndex[k - 1];
						}
						shareContainerCacheIndex[0] = tmp;
					}
				}
				/*if the share container is not cached in shareContainerCache*/
				else {
					/*move the evicted share container to the beginning of the cache*/
					if (numOfCachedShareContainers < NUM_OF_CACHED_CONTAINERS) {
						/*in this case, the evicted one is a new cache entry*/
						for (k = numOfCachedShareContainers; k > 0; k--) {
							shareContainerCacheIndex[k] = shareContainerCacheIndex[k - 1];
						}
						shareContainerCacheIndex[0] = numOfCachedShareContainers;
						numOfCachedShareContainers++;
					}
					else {
						/*in this case, the evicted one is the last cache entry*/
						int tmp = shareContainerCacheIndex[NUM_OF_CACHED_CONTAINERS - 1];
						for (k = NUM_OF_CACHED_CONTAINERS - 1; k > 0; k--) {
							shareContainerCacheIndex[k] = shareContainerCacheIndex[k - 1];
						}
						shareContainerCacheIndex[0] = tmp;
					}

					/*first read share container from the buffer; if it is not in the buffer, then read it from the disk. 
					  besides, store the new share container in the beginning of the cache to overwrite the evicted one*/
					if (!readShareContainerFromBuffer_(pShareIndexValueHead->shareContainerName, 
								shareContainerCache[shareContainerCacheIndex[0]].shareContainer)) {
						/*generate the full share container name*/
						fullShareContainerName = pShareIndexValueHead->shareContainerName;
						if (!addPrefixDir_(shareContainerDirName_, fullShareContainerName)) {
							fprintf(stderr, "Error: fail to add the prefix '%s' to '%s'!\n", 
									shareContainerDirName_.c_str(), fullShareContainerName.c_str());

							if (!recipeFileIsInBuffer) {
								fclose(recipeFilePointer);
							}

							free(recipeFileBuffer);
							free(shareFileBuffer);
							free(shareContainerCache);
							free(shareContainerCacheIndex); 

							delete inodeKeySlice;
							delete shareKeySlice;

							return 0;	
						}	

						/*open the container file for reading the share*/
						if (containerStorerObj_ != NULL) {
							containerStorerObj_->openOldFile(fullShareContainerName, containerFilePointer);
						}
						else {
							containerFilePointer = fopen(fullShareContainerName.c_str(), "rb");
						}

						if (containerFilePointer == NULL) {
							fprintf(stderr, "Error: fail to open the share container file '%s'!\n", 
									fullShareContainerName.c_str());

							if (!recipeFileIsInBuffer) {
								fclose(recipeFilePointer);
							}

							free(recipeFileBuffer);
							free(shareFileBuffer);
							free(shareContainerCache);
							free(shareContainerCacheIndex); 

							delete inodeKeySlice;
							delete shareKeySlice;

							return 0;	
						}

						if (fread(shareContainerCache[shareContainerCacheIndex[0]].shareContainer, 
									1, CONTAINER_BUFFER_SIZE, containerFilePointer) == 0){
							fprintf(stderr, "Error: fail to read the share container file '%s'!\n", 
									fullShareContainerName.c_str());

							if (!recipeFileIsInBuffer) {
								fclose(recipeFilePointer);
							}
							fclose(containerFilePointer);

							free(recipeFileBuffer);
							free(shareFileBuffer);
							free(shareContainerCache);
							free(shareContainerCacheIndex); 

							delete inodeKeySlice;
							delete shareKeySlice;

							return 0;	
						}

						fclose(containerFilePointer);
					}

					/*then update the cached share container name*/
					memcpy(shareContainerCache[shareContainerCacheIndex[0]].shareContainerName, 
							pShareIndexValueHead->shareContainerName, INTERNAL_FILE_NAME_SIZE);
				}	

				/*check if shareFileBuffer has enough space for keeping the share info and data*/
				if (shareFileBufferOffset + shareEntrySize_ + pShareIndexValueHead->shareSize > sentShareFileBufferSize) {
					/*add the message head before sending the data of the share file buffer*/
					indicator = htonl(-5);
					sentDataSize = htonl(shareFileBufferOffset - sentMsgHeadSize);
					memcpy(shareFileBuffer, &indicator, sizeof(uint32_t));
					memcpy(shareFileBuffer + sizeof(uint32_t), &sentDataSize, sizeof(uint32_t));

					/*send the data of the share file buffer through the socket with socketFD*/
					if ((sentSize = send(socketFD, shareFileBuffer, shareFileBufferOffset, 0)) != shareFileBufferOffset){
						fprintf(stderr, "Error: fail to send the data of the share file buffer (totally in %d bytes) \
								through the socket %d --- return %ld!\n", shareFileBufferOffset, socketFD, sentSize);

						if (!recipeFileIsInBuffer) {
							fclose(recipeFilePointer);
						}

						free(recipeFileBuffer);
						free(shareFileBuffer);
						free(shareContainerCache);
						free(shareContainerCacheIndex); 

						delete inodeKeySlice;
						delete shareKeySlice;

						return 0;	
					}

					/*reset shareFileBufferOffset*/
					shareFileBufferOffset = sentMsgHeadSize;
				}

				/*generate and store the share info into shareFileBuffer*/
				pShareEntry = (shareEntry_t *) (shareFileBuffer + shareFileBufferOffset);
				pShareEntry->secretID = pFileRecipeEntry->secretID;
				pShareEntry->secretSize = pFileRecipeEntry->secretSize;
				pShareEntry->shareSize = pShareIndexValueHead->shareSize;
				shareFileBufferOffset += shareEntrySize_;

				/*store the share data into shareFileBuffer*/
				memcpy(shareFileBuffer + shareFileBufferOffset, 
						shareContainerCache[shareContainerCacheIndex[0]].shareContainer + 
						pShareIndexValueHead->shareContainerOffset, pShareIndexValueHead->shareSize);
				shareFileBufferOffset += pShareIndexValueHead->shareSize;
			}

			/*if such a share does not exist*/
			if (shareStat.IsNotFound()) {	
				fprintf(stderr, "Error: cannot find a share for the key '%s' in the database!\n", 
						shareKeySlice->ToString().c_str());

				if (!recipeFileIsInBuffer) {
					fclose(recipeFilePointer);
				}

				free(recipeFileBuffer);
				free(shareFileBuffer);
				free(shareContainerCache);
				free(shareContainerCacheIndex); 

				delete inodeKeySlice;
				delete shareKeySlice;

				return 0;
			}

			if (shareStat.IsCorruption()) { 
				fprintf(stderr, "Error: a corruption error occurs for the key '%s' in the database!\n", 
						shareKeySlice->ToString().c_str());

				if (!recipeFileIsInBuffer) {
					fclose(recipeFilePointer);
				}

				free(recipeFileBuffer);
				free(shareFileBuffer);
				free(shareContainerCache);
				free(shareContainerCacheIndex); 

				delete inodeKeySlice;
				delete shareKeySlice;

				return 0;
			}

			if (shareStat.IsIOError()) {
				fprintf(stderr, "Error: an I/O error occurs for the key '%s' in the database!\n", 
						shareKeySlice->ToString().c_str());

				if (!recipeFileIsInBuffer) {
					fclose(recipeFilePointer);
				}

				free(recipeFileBuffer);
				free(shareFileBuffer);
				free(shareContainerCache);
				free(shareContainerCacheIndex); 

				delete inodeKeySlice;
				delete shareKeySlice;

				return 0;
			}

			delete shareKeySlice;
		}	

		if (shareFileBufferOffset > sentMsgHeadSize) {
			/*add the message head before sending the data of the share file buffer*/
			indicator = htonl(-5);
			sentDataSize = htonl(shareFileBufferOffset - sentMsgHeadSize);
			memcpy(shareFileBuffer, &indicator, sizeof(uint32_t));
			memcpy(shareFileBuffer + sizeof(uint32_t), &sentDataSize, sizeof(uint32_t));

			/*send the data of the share file buffer through the socket with socketFD*/
			if ((sentSize = send(socketFD, shareFileBuffer, shareFileBufferOffset, 0)) != shareFileBufferOffset){
				fprintf(stderr, "Error: fail to send the data of the share file buffer (totally in %d bytes) \
						through the socket %d --- return %ld!\n", shareFileBufferOffset, socketFD, sentSize);

				if (!recipeFileIsInBuffer) {
					fclose(recipeFilePointer);
				}

				free(recipeFileBuffer);
				free(shareFileBuffer);
				free(shareContainerCache);
				free(shareContainerCacheIndex); 

				delete inodeKeySlice;

				return 0;	
			}					
		}

		if (!recipeFileIsInBuffer) {
			fclose(recipeFilePointer);
		}

		free(recipeFileBuffer);
		free(shareFileBuffer);
		free(shareContainerCache);
		free(shareContainerCacheIndex); 
	}

	/*if such an inode for fullFileName does not exist*/
	if (inodeStat.IsNotFound()) {	
		fprintf(stderr, "Error: cannot find an inode for the key '%s' in the database!\n", 
				inodeKeySlice->ToString().c_str());

		delete inodeKeySlice;

		return 0;
	}

	if (inodeStat.IsCorruption()) { 
		fprintf(stderr, "Error: a corruption error occurs for the key '%s' in the database!\n", 
				inodeKeySlice->ToString().c_str());

		delete inodeKeySlice;

		return 0;
	}

	if (inodeStat.IsIOError()) {
		fprintf(stderr, "Error: an I/O error occurs for the key '%s' in the database!\n", 
				inodeKeySlice->ToString().c_str());

		delete inodeKeySlice;

		return 0;
	}

	delete inodeKeySlice;

	return 1;
}



