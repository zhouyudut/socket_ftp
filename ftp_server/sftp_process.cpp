#include "sftp.h"
#include "common.h"
#include <pthread.h>


//和ftp用户建立数据连接
void ftp_server::init(int sockfd)
{
	control_sock=sockfd;
	struct sockaddr_in address;
	socklen_t address_lenth=sizeof(address);
	memset(&address,0,sizeof(address));
	assert(getsockname(sockfd,(struct sockaddr*)&address,&address_lenth)==0);
	control_port=ntohs(address.sin_port);

	data_port=control_port+1;
	//printf("data port is %d\n",data_port);

	int data_listener=socket(PF_INET,SOCK_STREAM,0);
	assert(data_listener>=0);
	address.sin_port=htons(data_port);
	address.sin_family=AF_INET;
	printf("ip_address is %s\n",inet_ntoa(address.sin_addr));
	int ret=bind(data_listener,(struct sockaddr*)&address,sizeof(address));
	//printf("after bind\n");
	assert(ret!=-1);

	ret=listen(data_listener,5);
	//printf("after listen\n");
	assert(ret!=-1);

	struct sockaddr_in client;
	memset(&client,0,sizeof(client));
	socklen_t client_lenth=sizeof(client);
	//printf("before accept\n");
	data_sock=accept(data_listener,(struct sockaddr*)&client,&client_lenth);
	assert(data_sock!=-1);
	printf("data socket build over\n");

	//主从线程通信
	sem_init(&sem_a,0,1);
	sem_init(&sem_b,0,1);

	MAX_EVENT_NUMER=1024;

	auth=true;//暂且就不验证了

	init_thread();
}

ftp_server::ftp_server()
{
	auth=false;
	control_port=-1;
	data_port=-1;
	control_sock=-1;
	data_sock=-1;
	strcpy(path,"/home/zy/ftp/");
}

void ftp_server::init_thread()
{
	pthread_t pid;
	if(pthread_create(&pid,NULL,process_data,this)==-1)
	{
		//输出错误信息
		return;
	}
	pthread_detach(pid);
}

void ftp_server::process()
{
	Client_op_control client_op_struct;
	Server_re_control server_re_struct;
//	int ret=recv(control_sock,&client_op_struct,sizeof(client_op_struct),MSG_WAITALL);// TODO 有没有问题？？？
	int ret=recv(control_sock,&client_op_struct,sizeof(client_op_struct),0);
	printf("control_sock is %d ret =%d code is=%d\n",control_sock,ret,client_op_struct.CLIENT_OP_CODE);
	printf("file list is %d\n",FILELIST);
	if(ret<sizeof(client_op_struct))
		client_op_struct.CLIENT_OP_CODE=9000;

	Op_pool op_pool;
	Re_pool re_pool;
	switch (client_op_struct.CLIENT_OP_CODE)
	{
	case CHECKUSER:	/*验证用户名和账号*/
		auth=true;
		break;
	case GETDIR:	/*getdir data*/
		getDir();
		break;
	case CHANGEDIR:	/*changeDir control*/

		changeDir(client_op_struct.str);
		break;
	case ADDDIR:	/*makeDir data*/
		//makeDir();
		break;

	case FILELIST:	/*getFileList data*/
		printf("in file list\n");
		sem_wait(&sem_a);
			op_pool.exited=false;
			op_pool.OP_CODE=FILELIST;
			strcpy(op_pool.str,client_op_struct.str);
//		getFileList();
			op_queue.push(&op_pool);
		sem_post(&sem_a);
		break;
	case ADDFILE:	/*addFile data*/
//		addFile();
		sem_wait(&sem_a);
			op_pool.exited=false;
			op_pool.OP_CODE=ADDFILE;
			strcpy(op_pool.str,client_op_struct.str);
//		getFileList();
			op_queue.push(&op_pool);
		sem_post(&sem_a);
		break;
	case DELFILE:	/*delFile data*/
		delFile(client_op_struct.str);
		break;
	case GETFILE:	/*sendFile data*/
		//sendFile();
		break;
	default:	/*发送错误信息 control*/
		//发送错误的信息
		printf("???\n");
		break;
	}
	if(sem_wait(&sem_b)==0)
	{
		while(1)
		{
			if(re_queue.empty()==false)
			{
				Re_pool * re=re_queue.front();
				re_queue.pop();
				//TODO 将消息发送到客户端
				send(control_sock,re,sizeof(Re_pool),0);
			}
			else{
				break;
			}
		}
		sem_post(&sem_b);
	}

}

void ftp_server::stop()
{
	if(control_sock==-1)
		return ;
	else if(data_sock==-1)
	{
		close(control_sock);
		control_sock=-1;
		return ;
	}
	sem_wait(&sem_a);
	printf("before while\n");
	while(!op_queue.empty())
	{
		op_queue.pop();
	}
	printf("after while\n");
	Op_pool* op;
	op=new Op_pool;
	op->exited=true;
	op_queue.push(op);
	sem_post(&sem_a);

	Server_re_control re;
	re.SERVER_RE_CODE=EXIT_NOW;
	send(control_sock,(void*)&re,sizeof(re),0);
	exit_func();
	control_sock=-1;
	data_sock=-1;
}

void* ftp_server::process_data(void* arg)
{
	printf("in server thread\n");
	ftp_server* p=(ftp_server*)arg;
	while(1)
	{
		sem_wait(&p->sem_a);
		if(p->op_queue.empty()==true)
		{
			sem_post(&p->sem_a);
			continue;
		}
		Op_pool* op=p->op_queue.front();
		p->op_queue.pop();
		if(op->exited==true)
		{
//			close(control_sock);
//			close(data_sock);
			p->exit_func();
			sem_post(&p->sem_a);
			return NULL;
		}

		sem_post(&p->sem_a);
		switch (op->OP_CODE)
		{
		case ADDFILE:
			File_struct file_struct;
			//TODO 文件大小
			strcpy(file_struct.name,op->str);
			p->addFile(&file_struct);
			break;
		case GETDIR:
			p->getDir();
			break;
		case FILELIST:

			p->getFileList(op->str);
			break;
		}

	}
}
