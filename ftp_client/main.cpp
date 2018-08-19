#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include "cftp.h"
using namespace std;




int main(int argc,char* argv[])
{
    if(argc<2)
	{
		printf("usage: %s ip_address port\n",basename(argv[0]));
		return 0;
	}
	int control_port=atoi(argv[2]);
	int data_port=control_port+1;
	ftp_client* client;
	client=new ftp_client();
	if(client->init_client(argv[1],control_port,data_port))
		client->run();
    return 0;
}
