#include "cftp.h"
#include "common.h"

int ftp_client::my_recv(int sock,char* buf,int max_lenth)
{
	int idx=0;
	int ret;
	while(1)
	{
		ret=recv(sock,buf+idx,max_lenth-idx,0);
		if(ret==0)
		{
			exit_thread();
			exit_sock();
			break;
		}
		if(ret>0)
		{
			idx+=ret;
		}
		else if(ret<0)
		{
			if(errno!=EAGAIN && errno!=EINTR)
			{
				break;
			}
			else if(errno==EAGAIN || errno==EINTR)
			{
				continue;
			}
		}
		if(idx>=max_lenth)
			break;
	}
	if(ret==0)
		return ret;
	return idx;
}

int ftp_client::my_sig_recv(char* buf,int lenth)
{
	int ret;
	ret=recv(sig_pipefd[1],buf,lenth);
	return ret;
}


void ftp_client::exit_thread()
{
	//TODO 判断线程有没有退出
	if(pthread_status==false)
		return ;
	Op_pool * op;
	op=new Op_pool;
	op->exited=true;
	sem_wait(&sem_a);
	while(!op_queue.empty())
	{
		op_queue.pop();
	}
	op_queue.push(op);
	sem_post(&sem_a);
}

void ftp_client::exit_sock()
{
	close(control_sock);
	raise(SIGINT);
}
