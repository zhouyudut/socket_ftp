#include <iostream>
#include "processpool.h"
#include "sftp.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
using namespace std;

int get_listener(char* ip,char* cport)
{
	int port=atoi(cport);
	struct sockaddr_in address;
	memset(&address,0,sizeof(address));
	address.sin_family=AF_INET;
	address.sin_port=htons(port);
	inet_pton(PF_INET,ip,(struct sockaddr*)&address.sin_addr);

	int listener=socket(PF_INET,SOCK_STREAM,0);
	assert(listener>=0);

	int reuse=1;
	setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

	int ret=bind(listener,(struct sockaddr*)&address,sizeof(address));
//	printf("bind revalue is %d\n",errno);
	assert(ret!=-1);
	return listener;
}

int main(int argc,char* argv[])
{

    if(argc<3)
	{
		printf("usage: %s ip port\n",basename(argv[0]));
		return 0;
	}

	int listener=get_listener(argv[1],argv[2]);
	int ret=listen(listener,5);
	assert(ret!=-1);
	printf("build listener over\n");
	Processpool<ftp_server>* processpool=new Processpool<ftp_server>(listener);
	printf("build processpool over\n");
	processpool->run();
	close(listener);
	printf("ftp server over\n");

	delete processpool;
    return 0;
}
