#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include "common.h"

template<typename T>
class Process{
public:
	int epollfd;
	int id;
	pid_t pid;
	int pipefd[2];//pipefd[1]给父进程，pipefd[0]留给自己
	void user_init(int connfd);
	void user_process();
	void user_stop();
private:
	//	T* user;
	T* user;	//先一个进程对应着一个用户，开两个线程
};

template<typename T>
void Process<T>::user_init(int connfd)
{
	user=new T;
	user->init(connfd);
}

template<typename T>
void Process<T>::user_process()
{
	user->process();
}

template<typename T>
void Process<T>::user_stop()
{
	user->stop();
}

template<typename T>
class Processpool
{
private:
	static const int FD_LIMIT=65536;			//文件描述符限制
	static const int PROCESS_LIMIT=100;
	static const int EVENT_LIMIT=65536;		//事件限制
	static const int USER_LIMIT=400;			//连接的用户限制
	int process_num;
	int user_count;
	int listener;
	Process<T>* process;
	int cur_index;//目前下标，为了寻找合适的子进程
private:
	int epollfd;
	int id;			//坐标
	int pipefd[2];	//pipefd[1]父进程拥有，pipefd[0]子进程拥有
public:
	Processpool():user_count(0),listener(-1),process_num(-1),cur_index(-1){};
	Processpool(int listener,int process_num=16);
	~Processpool(){if(id==0) printf("parent process is over\n)");else printf("child process %d is over\n",id-1);};
	void run();
private:
	void run_child();
	void run_parent();
	void set_sig_pipe();
};

template<typename T>
Processpool<T>::Processpool(int listener,int process_num):listener(listener),process_num(process_num),user_count(0),cur_index(0)
{
	assert(process_num<=PROCESS_LIMIT);
	process=new Process<T>[process_num];
	assert(process);
	id=0;//父进程编号
	for(int i=0;i<process_num;i++)
	{
		int ret=socketpair(PF_UNIX,SOCK_STREAM,0,process[i].pipefd);
		assert(ret!=-1);
		pid_t pid=fork();
		assert(pid>=0);
		if(pid>0)
		{
			close(process[i].pipefd[0]);
			process[i].id=i+1;
			process[i].pid=pid;
			continue;
		}
		else{
			close(process[i].pipefd[1]);
			id=i+1;
			break;
		}
	}
}

template<typename T>
void Processpool<T>::set_sig_pipe()
{
	epollfd=epoll_create(20);
	assert(epollfd!=-1);

	int ret=socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);
	assert(ret!=-1);

	setnonblocking(sig_pipefd[0]);
	addfd(epollfd,sig_pipefd[1]);

	addsig(SIGCHLD,sig_handler);
	addsig(SIGTERM,sig_handler);
	addsig(SIGINT,sig_handler);
	addsig(SIGPIPE,sig_handler);
}

template<typename T>
void Processpool<T>::run()
{
	set_sig_pipe();//创建信号和epollfd
	if(id==0)
		run_parent();
	else
		run_child();
}

template<typename T>
void Processpool<T>::run_child()
{
	bool stop=false;
	addfd(epollfd,process[id-1].pipefd[0]);
	epoll_event events[EVENT_LIMIT];
	while(!stop)
	{
		int num=epoll_wait(epollfd,events,EVENT_LIMIT,-1);
		if(num<0 && errno!=EINTR)
		{
			printf("child process %d epoll failure\n",id);
			break;
		}
		for(int i=0;i<num;i++)
		{
			int fd=events[i].data.fd;
			if(fd==sig_pipefd[1] && events[i].events& EPOLLIN)
			{
				char msg[1000];
				int ret=recv(fd,msg,1000,0);
				if(ret<0)
					continue;
				for(int j=0;j<ret;j++)
				{
					switch (msg[j])
					{
					case SIGCHLD:
						pid_t pid;
						int stat;
						while((pid=waitpid(-1,&stat,WNOHANG))>0)
							continue;
						break;
					case SIGTERM:
					case SIGINT:
						{
					//		printf("in child process sig handler\n");
							process[id-1].user_stop();
						//	printf("user stop over\n");
							close(pipefd[0]);
							stop=true;
							break;
						}
					}
				}
			}
			else if(fd==process[id-1].pipefd[0] && events[i].events& EPOLLIN)
			{
				//新链接到来
				printf("new comming\n");
				char msg[4];
				int ret=recv(fd,msg,sizeof(msg),0);
				if(ret<0 && errno!=EINTR)
				{
					printf("child process %d can not get new connection\n",id);
					stop=true;
				}
				else
				{
					struct sockaddr_in client;
					socklen_t client_lenth=sizeof(client);
					int connfd=accept(listener,(struct sockaddr*)&client,&client_lenth);
					if(connfd<0)
					{
						continue;
					}
					process[id-1].user_init(connfd);//新的连接到来，那么就初始化一个新的用户，以后的话，这里肯定不是限定一个用户的
					addfd(epollfd,connfd);
				}
			}
			else if(events[i].events&EPOLLIN)
			{
				printf("client send msg to child process %d\n",id);
				process[id-1].user_process();
			}
		}
	}
	process[id-1].user_stop();
	close(listener);
}

template<typename T>
void Processpool<T>::run_parent()
{
//	listen(listener,5);
	bool stop=false;
	epoll_event events[EVENT_LIMIT];
	addfd(epollfd,listener);
	while(!stop)
	{
		int num=epoll_wait(epollfd,events,EVENT_LIMIT,-1);
		if(num<0 && errno!=EINTR)
		{
			printf("parent process epoll failure\n");
			break;
		}
		for(int i=0;i<num;i++)
		{
			int fd=events[i].data.fd;
			if(fd==sig_pipefd[1] && events[i].events&EPOLLIN)
			{
				char msg[1000];
				int ret=recv(fd,msg,1000,0);
				if(ret<=0 && errno!=EINTR)
				{
					continue;
				}
				for(int j=0;j<ret;j++)
				{
					switch (msg[j])
					{
					case SIGCHLD:
						{
							pid_t pid;
							int stat;
							while((pid=waitpid(-1,&stat,WNOHANG))>0)
							{
								for(int k=0;k<process_num;k++)
								{
									if(process[k].pid==pid)
									{
										process[k].pid=-1;
										printf("child process %d exit\n",k+1);
										close(process[k].pipefd[1]);
									}
								}
							}
							break;
						}
					case SIGTERM:
					case SIGINT:
						{
							printf("kill all the child process\n");
							for(int k=0;k<process_num;k++)
							{
								if(process[k].pid!=-1)
								{
									printf("parent will kill child process %d\n",k+1);
									kill(process[k].pid,SIGTERM);
								}
							}
							break;
						}
					}
				}

				bool flag=false;
				for(int j=0;j<process_num;j++)
				{
					if(process[j].pid!=-1)
					{
						flag=true;
						break;
					}
				}
				if(flag==false)
					stop=true;
			}
			else if(fd==listener && events[i].events&EPOLLIN)
			{
				printf("parent get new connection\n");
				int tmp_index;
				for(int j=0;j<process_num;j++)
				{
					tmp_index=(cur_index+j)%process_num;
					if(process[tmp_index].pid!=-1)
					{
						int conn=1;
						printf("send new connection to child process %d\n",j+1);
						send(process[tmp_index].pipefd[1],(char*)&conn,sizeof(conn),0);
						break;
					}
				}
				if(process[tmp_index].pid==-1)
				{
					printf("no child process is valid\n");
					stop=true;
					printf("parent process will exit\n");
					break;
				}
				else{
					cur_index=(tmp_index+1)%process_num;
				}
			}
		}
	}
	close(listener);
}
#endif
