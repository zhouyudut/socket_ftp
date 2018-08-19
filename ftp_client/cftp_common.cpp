#include "cftp.h"
#include "type.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "common.h"

void ftp_client::sendFile(char* filename)
{
	struct stat statbuf;
	stat(filename,&statbuf);

	File_struct file;
	file.fileSize=statbuf.st_size;
	strcpy(file.name,filename);
	file.isDir=false;

	//send(data_sock,&file,sizeof(file),0);
	char buf[2048];
	int fd;
	int file_size;
	if(access(filename,R_OK)!=0 || (fd=open(filename,O_RDONLY))<0)
	{
		file.fileSize=-1;
		send(data_sock,&file,sizeof(file),0);
		sem_wait(&sem_b);
		Re_pool re;
		re.RE_CODE=FAILED;

		strcpy(re.message,"can not read file");
		re_queue.push(&re);
		sem_post(&sem_b);
		return ;
	}

	file_size=file.fileSize;
	while(file_size>0)
	{
		if(file_size>1024)
		{
			read(fd,buf,1024);
			send(data_sock,buf,sizeof(buf),0);
			file_size-=1024;
		}
		else
		{
			read(fd,buf,file_size);
			send(data_sock,buf,sizeof(buf),0);
			file_size=0;
		}
	}
	close(fd);

	Re_pool re;
	re.RE_CODE=SUCCESS;
	strcpy(re.message,"send file ok");
	sem_wait(&sem_b);
	re_queue.push(&re);
	sem_post(&sem_b);
}


void ftp_client::GetFileList(char* mypath)//需要传参嘛？？？
{
	Re_pool re;
	Dir_struct dir;

	printf("my dir path is %s\n",mypath);
	int ret=my_recv(data_sock,(char*)&dir,sizeof(dir));
	if(ret==0)
		return ;
	printf("after recv dir\n");
	if(dir.isERROR==true)
	{
		re.RE_CODE=FAILED;
        strcpy(re.message,"directory error\n");
        sem_wait(&sem_b);
        re_queue.push(&re);
        sem_post(&sem_b);
        return ;
	}
	printf("%s has %d items\n",dir.name,dir.num);
	File_struct *files=(File_struct*)malloc(sizeof(File_struct)*dir.num);
	if(files==NULL)
	{
        re.RE_CODE=FAILED;
        strcpy(re.message,"can not show file list");
		sem_wait(&sem_b);
        re_queue.push(&re);
        sem_post(&sem_b);
        return ;
	}

	int recv_size=0;
	char* buf=(char*)files;

	ret=my_recv(data_sock,(char*)files,sizeof(File_struct)*dir.num);
	if(ret==0)
		return ;
	printf("id\tname\ttype\tsize\n");
	for(int i=0;i<dir.num;i++)
	{
		printf("%d\t%s\t%s\t%d\n",files[i].id,files[i].name,(files[i].isDir==true)?"dir":"file",files[i].fileSize);
	}
	re.RE_CODE=SUCCESS;
	strcpy(re.message,"file list over");
	sem_wait(&sem_b);
	re_queue.push(&re);
	sem_post(&sem_b);
}

void ftp_client::read_stdin()
{
	char op_code[200];
	char op_str[200];

	memset(op_code,'\0',sizeof(op_code));
	memset(op_str,'\0',sizeof(op_str));

	memset(send_struct.str,0,sizeof(char)*200);
	memset(send_struct.other,0,sizeof(char)*200);
	memset(send_struct.test_msg,0,sizeof(char)*200);
	if(fscanf(stdin,"%s %s",op_code,op_str)!=2)//以后可以添加多个str，比如发送多个文件
	{
		printf("input error\n");
		//TODO;
		send_struct.CLIENT_OP_CODE=-1111;
		return ;
	}
	printf("%s %s\n",op_code,op_str);
	setbuf(stdin,NULL);

	send_struct.CLIENT_OP_CODE=-1111;

	//获取服务器上的文件
	if(strcasecmp(op_code,"GET")==0 || strcasecmp(op_code,"RECV")==0 || strcasecmp(op_code,"PULL")==0)
	{
		send_struct.CLIENT_OP_CODE=GETFILE;
		if(op_str[0]=='\0')
		{
			send_struct.CLIENT_OP_CODE=-1111;
			return ;
		}
		if(op_str[0]=='/')
		{
			strcpy(send_struct.str,op_str);
		}
		else{
			sprintf(send_struct.str,"%s/%s",remote_path,basename(op_str));
		}
	}
	//重命名服务器上文件名  TODO
	else if (strcasecmp(op_code,"RENAMEFILE")==0 || strcasecmp(op_code,"RENAMEFILENAME")==0)
	{
		//op.OP_CODE=RENAMEFILE;

		//TODO   待升级
	}
	//删除服务器上的文件
	else if(strcasecmp(op_code,"DELETE")==0 || strcasecmp(op_code,"DEL")==0 || strcasecmp(op_code,"REMOVE")==0 )
	{
		send_struct.CLIENT_OP_CODE=DELFILE;
		if(op_str[0]=='\0')
		{
			send_struct.CLIENT_OP_CODE=-1111;
			return ;
		}
		if(op_str[0]=='/')
		{
			strcpy(send_struct.str,op_str);
		}
		else
		{
			sprintf(send_struct.str,"%s/%s",remote_path,basename(op_str));
		}
	}
	//将本地文件发送到服务器上
	else if(strcasecmp(op_code,"ADD")==0 || strcasecmp(op_code,"SEND")==0 || strcasecmp(op_code,"PUSH")==0 || strcasecmp(op_code,"PUT")==0)
	{
		send_struct.CLIENT_OP_CODE=ADDFILE;
		if(op_str[0]=='\0')
		{
			send_struct.CLIENT_OP_CODE=-1111;
			return ;
		}
		if(op_str[0]=='/')
		{
			strcpy(send_struct.str,op_str);
		}
		else
		{
			sprintf(send_struct.str,"%s/%s",remote_path,basename(op_str));
		}
	}
	//在远程创建文件夹
	else if (strcasecmp(op_code,"SMKDIR")==0 )
	{
		send_struct.CLIENT_OP_CODE=ADDDIR;
		if(op_str[0]=='\0')
		{
			send_struct.CLIENT_OP_CODE=-1111;
			return ;
		}
		if(op_str[0]=='/')
		{
			strcpy(send_struct.str,op_str);
		}
		else
		{
			sprintf(send_struct.str,"%s/%s",remote_path,basename(op_str));
		}
	}
	//展示文件夹内容
	else if(strcasecmp(op_code,"SHOW")==0 || strcasecmp(op_code,"LIST")==0 || strcasecmp(op_code,"FILELIST")==0 || strcasecmp(op_code,"GETFILELIST")==0)
	{
		send_struct.CLIENT_OP_CODE=FILELIST;
		if(op_str[0]=='\0')
		{
			send_struct.CLIENT_OP_CODE=-1111;
			return ;
		}
		if(op_str[0]=='/')
		{
			strcpy(send_struct.str,op_str);
		}
		//TODO未来可以做一个目录的升级 比如./就是上一个目录
		//目前只做到了..是当前目录
		else if(op_str[0]=='.' && op_str[1]=='.')
		{
			strcpy(send_struct.str,remote_path);
		}
		else
		{
			sprintf(send_struct.str,"%s/%s",remote_path,basename(op_str));
		}
	}
	//更改服务器的路径
	else if(strcasecmp(op_code,"CHANGEDIR")==0 || strcasecmp(op_code,"CHANGEDIRNAME")==0)
	{
		send_struct.CLIENT_OP_CODE=CHANGEDIR;
		if(op_str[0]=='\0')
		{
			send_struct.CLIENT_OP_CODE=-1111;
			return ;
		}
		if(op_str[0]=='/')
		{
			strcpy(send_struct.str,op_str);
		}
		else
		{
			sprintf(send_struct.str,"%s/%s",remote_path,basename(op_str));
		}
	}
	//重命名服务器上文件夹路径名
	else if(strcasecmp(op_code,"RENAMEDIR")==0 || strcasecmp(op_code,"RENAMEDIRNAME")==0)
	{
		//TODO
	}
}


void ftp_client::work_process()
{
	Op_pool op;
	if(send_struct.CLIENT_OP_CODE<0)
	{
		printf("%d \n",raise(SIGINT));
		printf("in raise\n");
		return ;
	}
	switch(send_struct.CLIENT_OP_CODE)
	{
	case DELFILE:
	case RENAMEFILE:
	case GETDIR:
	case CHANGEDIR:
	case ADDDIR:
	case DELDIR:
	case RENAMEDIR:
		send(control_sock,&send_struct,sizeof(send_struct),0);
		break;
	case ADDFILE:
	case GETFILE:
	case FILELIST:
		printf("in work process data switch\n");
		send(control_sock,&send_struct,sizeof(send_struct),0);
		//op.exited=true;
		op.exited=false;
		op.OP_CODE=send_struct.CLIENT_OP_CODE;
		strcpy(op.str,send_struct.str);
		sem_wait(&sem_a);
		op_queue.push(&op);
		sem_post(&sem_a);
		break;
	}
}

void ftp_client::recv_result()
{
	int ret;
	if(send_struct.CLIENT_OP_CODE<0)
	{
		return ;
	}
	memset(&recv_struct,0,sizeof(recv_struct));
	switch(send_struct.CLIENT_OP_CODE)
	{
	case DELFILE:
	case RENAMEFILE:
	case GETDIR:
	case CHANGEDIR:
	case ADDDIR:
	case DELDIR:
	case RENAMEDIR:
		ret=my_recv(control_sock,(char*)&recv_struct,sizeof(recv_struct));
		break;
	case ADDFILE:
	case GETFILE:
	case FILELIST:
		ret=my_recv(control_sock,(char*)&recv_struct,sizeof(recv_struct));
		break;
	}
	if(ret<=0)
	{
		return ;
	}
	char result[200];
	if(recv_struct.SERVER_RE_CODE==SUCCESS)
	{
		strcpy(result,"success");
	}
	else{
		strcpy(result,"failed");
	}
	switch(send_struct.CLIENT_OP_CODE)
	{
	case DELFILE:
		printf("delete file %s\n",result);
		break;
	case RENAMEFILE:
		printf("rename file %s\n",result);
		break;
	case GETDIR:
		printf("get dir %s\n",result);
		break;
	case CHANGEDIR:
		printf("change dir %s\n",result);
		break;
	case ADDDIR:
		printf("add dir %s\n",result);
		break;
	case DELDIR:
		printf("delete dir %s\n",result);
		break;
	case RENAMEDIR:
		printf("rename dir %s\n ",result);
		break;
	case ADDFILE:
		printf("add file %s\n",result);
		break;
	case GETFILE:
		printf("add file %s\n",result);
		break;
	case FILELIST:
		printf("list file %s\n",result);
		break;
	}

	if(recv_struct.SERVER_RE_CODE==FAILED)
	{
		printf("\t%s\n",recv_struct.msg);
	}
	if(sem_trywait(&sem_b)==0)
	{
		while(!re_queue.empty())
		{
			Re_pool* re=re_queue.front();
			re_queue.pop();
			if(re->RE_CODE==SUCCESS)
			{
				printf("success\n");
			}
			else{
				printf("error\n");
			}

		}
	}
}

void ftp_client::GetDir()
{
	Client_op_control cop;
	Server_re_control src;

	cop.CLIENT_OP_CODE=GETDIR;
	send(control_sock,&cop,sizeof(cop),0);
//	recv(control_sock,&cop,sizeof(cop),MSG_WAITALL);
//	printf("test msg is %s\n",cop.test_msg);
	int idx=my_recv(control_sock,(char*)&src,sizeof(src));
	if(idx==0)
	{
		printf("recv path error\n");
		return ;
	}
	printf("recv lenth is %d \nremote_path is %s\n",idx,src.msg);
	strcpy(remote_path,src.msg);
}


