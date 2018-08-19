#include "cftp.h"
#include "type.h"
#include <stdlib.h>
#include "common.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

ftp_client::ftp_client()
{
	control_sock=-1;
	data_sock=-1;
	memset(local_path,0,sizeof(local_path));
	memset(remote_path,0,sizeof(remote_path));
	sem_init(&sem_a,0,1);
	sem_init(&sem_b,0,1);
}
ftp_client::~ftp_client()
{
	control_sock=-1;
	data_sock=-1;
	sem_destroy(&sem_a);
	sem_destroy(&sem_b);
}
bool ftp_client::init_client(char* ip,int control_port,int data_port)
{
	printf("ip: %s control_port: %d data_port: %d\n",ip,control_port,data_port);

	struct sockaddr_in control_address,data_address;
	memset(&control_address,0,sizeof(control_address));
	memset(&data_address,0,sizeof(data_address));

	control_address.sin_family=AF_INET;
	data_address.sin_family=AF_INET;
	control_address.sin_port=htons(control_port);
	data_address.sin_port=htons(data_port);
	inet_pton(AF_INET,ip,&control_address.sin_addr);
	inet_pton(AF_INET,ip,&data_address.sin_addr);

	control_sock=socket(PF_INET,SOCK_STREAM,0);
	data_sock=socket(PF_INET,SOCK_STREAM,0);
	assert(control_sock>=0 && data_sock>=0);

	if(connect(control_sock,(struct sockaddr*)&control_address,sizeof(control_address))<0)
	{
		printf("connect control sock error\n");

		return false;
	}
	sleep(1);	//给服务器一点时间
	if(connect(data_sock,(struct sockaddr*)&data_address,sizeof(data_address))<0)
	{
			printf("connect data sock error\n");
			return false;
	}
	else{
		printf("build connection\n");
	}

	strcpy(local_path,"/home/zy/data/");
	return true;
}

void ftp_client::run()
{
	pthread_create(&pid,NULL,run_data,this);
	pthread_detach(pid);
	pthread_status=true;
	run_control();
}


void* ftp_client::run_data(void* arg)
{
	ftp_client* fc=(ftp_client*)arg;
	Op_pool* op;
	Re_pool re;
	while(1)
	{
		sem_wait(&fc->sem_a);
			if(fc->op_queue.empty())
			{
				sem_post(&fc->sem_a);
				sleep(1);
				continue;
			}
		op=fc->op_queue.front();
		fc->op_queue.pop();
		sem_post(&fc->sem_a);

		if(op->exited==true)
		{
			// TODO 执行退出操作
			close(data_sock);
			break;
		}
		switch (op->OP_CODE)
		{
		case ADDFILE:
			//send file
			break;
		case DELFILE:
			//send file name
			break;
		case FILELIST:
			//send dir name
			printf("ftp client get file list function\n");
			fc->GetFileList(op->str);
			break;
		case GETDIR:
			// don't need this
			break;
		}
	}
	pthread_status=false;
	printf("ftp thread exit\n");
	return NULL;
}

void ftp_client::run_control()
{
	epoll_event events[1024];
	int epollfd=epoll_create(5);
	addfd(epollfd,control_sock);
	addfd(epollfd,STDIN_FILENO);//试下这个 直接通过监听来判断接收

    int ret=socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);
    assert(ret!=-1);

    setnonblocking(sig_pipefd[0]);
    addfd(epollfd,sig_pipefd[1]);

    addsig(SIGHUP,sig_handler);
    addsig(SIGCHLD,sig_handler);
    addsig(SIGTERM,sig_handler);
    addsig(SIGINT,sig_handler);

    bool stop;
    stop=false;

    //TODO首先获取下服务器端当前的目录
	GetDir();

    while(!stop)
	{
		int num=epoll_wait(epollfd,events,1024,-1);
		if(num<0 && errno!=EINTR)
		{
			printf("epoll failure\n");
			break;
		}
		printf("num is %d\n",num);
		for(int i=0;i<num;i++)
		{
			int fd=events[i].data.fd;

			if(fd==STDIN_FILENO)
			{
				//处理标注输入
				read_stdin();
				work_process();
//				printf("after work process\n");
			}
			else if(fd==control_sock)
			{
//				printf("in control sock\n");
				recv_result();//TODO 这些应该要判断 对方要求自己退出，或者重连。
			}
			else if(fd==sig_pipefd[1] && events[i].events& EPOLLIN)
			{
				int sig;
				char signals[1024];
//				printf("before recv signals\n");
				ret=my_sig_recv(signals,sizeof(signals),0);
//				printf("recv %d signals",ret);
				if(ret<=0)
				{
					continue;
				}
				else{
					for(int j=0;j<ret;j++)
					{
						switch(signals[j])
						{
						case SIGCHLD:
						case SIGHUP:
							{
								continue;
							}
						case SIGTERM:
						case SIGINT:
							{
								printf("in sigint\n");
								exit_thread();
								stop=true;
							}
						}
					}
				}
			}
		}
	}
	printf("ftp process exit\n");
}
