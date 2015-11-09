#include "server.hh"
#include "DedupCore.hh"
#include "CryptoPrimitive.hh"

using namespace std;

DedupCore* dedupObj;

Server* server;

int main(int argv, char** argc){

	/* enable openssl locks */
	if (!CryptoPrimitive::opensslLockSetup()) {
		printf("fail to set up OpenSSL locks\n");

		exit(1); 
	}

	/* initialize objects */
	BackendStorer* recipeStorerObj = NULL;
	BackendStorer* containerStorerObj = NULL;
	dedupObj = new DedupCore("./","meta/DedupDB","meta/RecipeFiles","meta/ShareContainers",recipeStorerObj, containerStorerObj);

	/* initialize server object */
	server = new Server(atoi(argc[1]), dedupObj);

	/* run server service */
	server->runReceive();

	/* openssl lock cleanup */
	CryptoPrimitive::opensslLockCleanup();

	return 0;
}
