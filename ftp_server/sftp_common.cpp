/*
	void process_control();	//处理控制数据的线程函数
	void process_data();	//处理传输数据的线程函数
	void changeDir(char*);	//control
	void checkUser();		//control
	void getDir();			//data
	void getFileList();		//data
	void addFile();			//data
	void delFile();			//control
*/
#include "sftp.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <io.h>
using namespace std;



//data
void ftp_server::getDir()
{
	printf("send path to client\n");
//	printf("control_sock is %d path is %s\n",control_sock,path);
	Client_op_control cop;
	Server_re_control src;
	strcpy(src.msg,path);
	src.SERVER_RE_CODE=SUCCESS;
	src.SERVER_OP_CODE=GETDIR;

	int ret=send(control_sock,&src,sizeof(src),0);	//TODO  这里有没有问题？？？？
//	printf("send lenth is %d\n",ret);
}

//data
void ftp_server::getFileList(char* dirname)
{
	int count=0;
	char* path_name=NULL;
	struct dirent* ent=NULL;
	DIR *dir;
	struct stat statbuf;

	Dir_struct mydir;
	File_struct* myfiles;
	strcpy(mydir.name,dirname);
	mydir.isERROR=true;
	mydir.num=-1;

	printf("in server get file list function\n");

//打开目录
	if((dir=opendir(dirname))==NULL)
	{
		send(data_sock,&mydir,sizeof(mydir),0);//发送路径不存的信息
		printf("dir is null\n");
		return ;
	}
	printf("after check dir\n");

//获取目录下文件或文件夹个数
	count=0;
	while((ent=readdir(dir))!=NULL)
	{
		count++;
	}

	printf("after check file num\n");
	mydir.num=count;
	closedir(dir);

	myfiles=(File_struct*)malloc(sizeof(File_struct)*count);
	if(myfiles==NULL)
	{
		printf("in func :%s\nline in : %d\n",__FILE__,__LINE__);
		printf("malloc myfiles error\n");
		send(data_sock,(char*)&mydir,sizeof(mydir),0);//主机无法发送文件的具体信息。
		return ;
	}

	dir=opendir(path);//默认再次打开不会有问题
	printf("after open dir again\n");

	char tmp[1024];
	for(int i=0;i<count;i++)//默认文件夹下的文件或文件夹并没有发生改变
	{
		ent=readdir(dir);
		strcpy(myfiles[i].name,ent->d_name);
		printf("%s\n",ent->d_name);
		myfiles[i].id=i;
		myfiles[i].isDir=true;
		myfiles[i].fileSize=-1;

		snprintf(tmp,512,"%s/%s",path,ent->d_name);
		stat(tmp,&statbuf);
		myfiles[i].fileSize=statbuf.st_size;
	//	lstat(tmp,&statbuf);
		if(!S_ISDIR(statbuf.st_mode))
		{
			myfiles[i].isDir=false;
		}
	}
	closedir(dir);
	printf("after for loop \n");
	mydir.isERROR=false;
	send(data_sock,(char*)&mydir,sizeof(mydir),0);
	send(data_sock,(char*)myfiles,sizeof(File_struct)*count,0);
	printf("after send over\n");
	sem_wait(&sem_b);
	Re_pool re;
	re.RE_CODE=SUCCESS;
	re_queue.push(&re);
	sem_post(&sem_b);
}

//data
void ftp_server::addFile(File_struct* file)
{
	char filename[2048];
	unsigned long file_size;

	strcpy(filename,file->name);
	file_size=file->fileSize;

	//TODO 判断文件名是否合法
	if(access(filename,F_OK)==0)
	{
		//TODO 线程通知主线程，不可以写文件
		Re_pool re;
		re.RE_CODE=FAILED;
		strcpy(re.message,"add file error,file name erro\n");
		sem_wait(&sem_b);
		re_queue.push(&re);
		sem_post(&sem_b);
		return ;
	}

	int fd=open(filename,O_WRONLY | O_CREAT);
	char* tmp=filename;
	while(1)
	{
		int num=read(data_sock,tmp,2048);
		if((file_size-num)<=0)
		{
			break;
		}
		file_size-=num;
		write(fd,(void*)tmp,num);
	}
	close(fd);

	sem_wait(&sem_b);
	Re_pool re;
	re.RE_CODE=SUCCESS;
	re_queue.push(&re);
	sem_post(&sem_b);
}

void ftp_server::delFile(char* filename)
{

	//TODO 判断文件名是否合法
	if(access(filename,F_OK)!=0)
	{
		//TODO 线程通知主线程，不可以写文件
		Re_pool re;
		re.RE_CODE=FAILED;
		strcpy(re.message,"delete file error,filename error\n");
		sem_wait(&sem_b);
		re_queue.push(&re);
		sem_post(&sem_b);
		return ;
	}

	//TODO 确保数据同步
	remove(filename);
	//TODO 通知主线程删除了，

	Re_pool re;
	re.RE_CODE=SUCCESS;
	sem_wait(&sem_b);
	re_queue.push(&re);
	sem_post(&sem_b);
}


//control
void ftp_server:: changeDir(char* dir)
{
	if(dir==NULL || access(dir,F_OK)!=0)
	{
		// TODO 应该要有一个状态信息传递给process_control
		return;
	}
	if(access(dir,F_OK)==0)
	{
		strcpy(path,dir);
	}
}

void ftp_server::exit_func()
{
	close(control_sock);
	close(data_sock);
}
