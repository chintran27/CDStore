

# CDSTORE

 * Introduction
 * Requirements
 * Installation
 * Configuration
 * Make
 * Example
 * Maintainers

# INTRODUCTION

CDStore builds on an augmented secret sharing scheme called convergent dispersal, which supports deduplication by using deterministic content-derived hashes as inputs to secret sharing. It combines convergent dispersal with two-stage deduplication to achieve both bandwidth and storage savings and be robust against side-channel attacks. 


# REQUIREMENTS

CDStore is built on Ubuntu 12.04.3 LTS with gcc version 4.6.3.

This software requires the following libraries:

 * OpenSSL (https://www.openssl.org/source/openssl-1.0.2a.tar.gz)
 * GF-Complete (https://github.com/ceph/gf-complete/archive/master.zip)
 * boost C++ library (http://sourceforge.net/projects/boost/files/boost/1.58.0/boost_1_58_0.tar.gz)
 * LevelDB (https://github.com/google/leveldb/archive/master.zip)

The GF-Complete and LevelDB are packed in /client/lib/ and /server/lib/ respectively.


# INSTALLATION


For linux user you can install the LevelDB dependency, OpenSSL and Boost by the following:

 * sudo apt-get install libssl1.0.0 libboost-all-dev libsnappy-dev


# CONFIGURATION


 * Configure the server (CDStore requires at least 4 storage server)

	- Make sure there are three directories under /server/meta
	- "DedupDB" for levelDB logs
	- "RecipeFiles" for temp recipe files
	- "ShareContainers" for share local cache
	- Start a server by "./SERVER [port]"

 * Configure the client

	- In the configure file /client/config, specify the storage nodes line by line with [IP]:[port]

	Example: you have run 4 servers with "./SERVER [port]" on machines:
		- 192.168.0.30 with port 11030
		- 192.168.0.31 with port 11031
		- 192.168.0.32 with port 11032
		- 192.168.0.33 with port 11033
		
		you need to specify the ip and port in config with following format: 

			192.168.0.30:11030
			192.168.0.31:11031
			192.168.0.32:11032
			192.168.0.33:11033

		(the actual order doesn't matter)
	
	- (Optional) In the configure class of client, /client/util/conf.hh

		- set chunk and secure parameters following the comments
		- set the number of storage nodes according to your running servers

# MAKE


	To make a client, on the client machine:

	 * Go to /client/lib/, type "make" to make gf_complete
	 * Back to /client/, type "make" to get the executable CLIENT program

	To make a server, on each storage node:

	 * Go to /server/lib/leveldb/, type "make" to make levelDB
	 * Back to /server/, type "make" to get the executable SERVER program



# EXAMPLE

 * After successful make

	usage: ./CLIENT [filename] [userID] [action] [secutiyType]

	- [filename]: full path of the file;
	- [userID]: user ID of current client;
	- [action]: [-u] upload; [-d] download;
	- [securityType]: [HIGH] AES-256 & SHA-256; [LOW] AES-128 & SHA-1


 * To upload a file "test", assuming from user "0" using AES-256 & SHA-256

	./CLIENT test 0 -u HIGH

 * To download a file "test", assuming from user "1" using AES-128 & SHA-1

	./CLIENT test 1 -d LOW



# MAINTAINER

 * Current maintainer

	- Chuan QIN, the Chinese University of Hong Kong, chintran27@gmail.com

 * Original maintainer

	- Mingqiang Li, Lenovo Hong Kong, mingqianglicn@gmail.com




