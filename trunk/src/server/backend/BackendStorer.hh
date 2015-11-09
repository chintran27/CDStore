/*
 * BackendStorer.hh
 */

#ifndef __BACKENDSTORER_HH__
#define __BACKENDSTORER_HH__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/*for the use of boost bimap*/
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp> 
#include <boost/bimap/list_of.hpp> 

/*macro for the name size of internal files (recipe or container files)*/
#define INTERNAL_FILE_NAME_SIZE 16

/*macro for the type of backend storage*/
#define IPSERVER 0
#define AMAZON 1
#define GOOGLE 2
#define MSAZURE 3
#define RACKSPACE 4

using namespace std;

/*all cache-related data types*/
typedef boost::bimap< \
			boost::bimaps::set_of<std::string>, \
			boost::bimaps::list_of<long> \
			> cache_t;
typedef cache_t::value_type cacheEntry_t;
typedef cache_t::left_iterator lIterator_t;
typedef cache_t::right_iterator rIterator_t;

/*the file node structure*/
typedef struct fileNode{
	char shortFileName[INTERNAL_FILE_NAME_SIZE];
	long fileSize;
	struct fileNode *next;
} fileNode_t;

class BackendStorer{
	private:	
		/*the type of backend storage*/
		int backendType_;
		std::string backendDirName_;
		std::string backendIP_;

		/*variables for cache*/
		std::string cacheDirName_;
		long maxCacheSize_;		
		long availCacheSize_;
		long usedCacheSize_;
		cache_t cache_;

		/*variables for storing new files*/
		fileNode_t *headNewlyStoredFileNode_;
		fileNode_t *tailNewlyStoredFileNode_;
		int numOfNewlyStoredFiles_;
		pthread_mutex_t fileStorerLock_;
		pthread_cond_t fileStorerCond_;

		/*variables for updating cache*/
		fileNode_t *headNewlyUsedFileNode_;
		fileNode_t *tailNewlyUsedFileNode_;
		int numOfNewlyUsedFiles_;
		pthread_mutex_t cacheUpdaterLock_;
		pthread_cond_t cacheUpdaterCond_;

		/*the size of a file node*/
		int fileNodeSize_;

		/*
		 * format a directory name into '.../.../shortName/'
		 *
		 * @param dirName - the directory name to be formated <return>
		 *
		 * @return - a boolean value that indicates if the formating succeeds
		 */
		bool formatDirName_(std::string &dirName);

		/*
		 * format an IP address
		 *
		 * @param IP - the IP address to be formated <return>
		 *
		 * @return - a boolean value that indicates if the formating succeeds
		 */
		bool formatIP_(std::string &IP);

		/*
		 * check if two directory names are equivalent
		 *
		 * @param dirName1 - the first directory name
		 * @param dirName2 - the second directory name
		 *
		 * @return - a boolean value that indicates if the two directory names are equivalent
		 */
		bool equDirName_(const std::string &dirName1, const std::string &dirName2);

		/*
		 * shorten a full file name into a short file name
		 *
		 * @param fullFileName - the full file name (including the path)
		 * @param shortFileName - the short file name (excluding the path) <return>
		 *
		 * @return - a boolean value that indicates if the shortening op succeeds
		 */
		bool shortenFileName_(const std::string &fullFileName, std::string &shortFileName);

		/*
		 * check if a file exists
		 *
		 * @param fullFileName - the full file name
		 *
		 * @return - a boolean value that indicates if the file exists
		 */
		inline bool checkFileExistence_(const std::string &fullFileName);

		/*
		 * check if a file can be accessed with all permissions
		 *
		 * @param fullFileName - the full file name
		 *
		 * @return - a boolean value that indicates if the file can be accessed with all permissions
		 */
		inline bool checkFilePermissions_(const std::string &fullFileName);

		/*
		 * get the size of an opened file
		 *
		 * @param fp - the file pointer
		 *
		 * @return - the size of the file
		 */
		inline long getOpenedFileSize_(FILE *fp);

		/*
		 * get the size of an unopened file
		 *
		 * @param fullFileName - the full file name
		 *
		 * @return - the size of the file
		 */
		inline long getUnopenedFileSize_(const std::string &fullFileName);

		/*
		 * locally copy data from the file with srcFullFileName to the file with destFullFileName
		 *
		 * @param destFullFileName - the full name of the destination file
		 * @param srcFullFileName - the full name of the source file
		 *
		 * @return - a boolean value that indicates if the copy op succeeds
		 */
		bool localCopyFile_(const std::string &destFullFileName, const std::string &srcFullFileName);

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
		bool IPCopyFile_(const std::string &destIP, const std::string &destFullFileName, 
				const std::string &srcIP, const std::string &srcFullFileName);

		/*
		 * store a new file into the backend storage 
		 *
		 * @return - a boolean value that indicates if the store op succeeds
		 */
		bool backendStoreNewFile_(const std::string &shortFileName);

		/*
		 * restore a new file from the backend storage 
		 *
		 * @return - a boolean value that indicates if the restore op succeeds
		 */
		bool backendRestoreOldFile_(const std::string &shortFileName, FILE *&fp);

	public:
		/*
		 * constructor of BackendStorer
		 *
		 * @param cacheDirName - the name of the directory that acts as a cache
		 * @param maxCacheSize - the maximum cache size
		 * @param backendIP - the IP of the server that keeps the backend storage
		 * @param backendDirName - the name of the directory that acts as the backend storage
		 */
		BackendStorer(const std::string &cacheDirName, long maxCacheSize, 
				const std::string &backendIP, const std::string &backendDirName);

		/*
		 * constructor of BackendStorer
		 *
		 * @param cacheDirName - the name of the directory that acts as a cache
		 * @param maxCacheSize - the maximum cache size
		 * @param backendType - the type of the backend storage (AMAZON, GOOGLE, MSAZURE, or RACKSPACE)
		 */
		BackendStorer(const std::string &cacheDirName, long maxCacheSize, int backendType);

		/* 
		 * destructor of BackendStorer 
		 */
		~BackendStorer();

		/*
		 * add a new file
		 *
		 * @param fullFileName - the full file name
		 *
		 * @return - a boolean value that indicates if the add op succeeds
		 */
		bool addNewFile(const std::string &fullFileName);

		/*
		 * open an old file
		 *
		 * @param fullFileName - the full file name
		 * @param fp - the file pointer <return>
		 *
		 * @return - a boolean value that indicates if the open op succeeds
		 */
		bool openOldFile(const std::string &fullFileName, FILE *&fp);

		/*
		 * store each new file into the backend storage 
		 */
		void storeNewFile();

		/*
		 * update the status of the cache
		 */
		void updateCache();
};

#endif
