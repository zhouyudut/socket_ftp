#ifndef SFTP_H
#define SFTP_H
#include <semaphore.h>
#include "type.h"
#include <iostream>
#include <queue>
#include <stdlib.h>
#include <string.h>

class ftp_server
{
public:
	ftp_server();
	~ftp_server();
	void init(int fd);
	void process();//	主要处理流程
	void stop();
private:
	sem_t sem_a,sem_b;
	std::queue<Op_pool*> op_queue;
	std::queue<Re_pool*> re_queue;
private:
	bool auth;			//用户是否验证
	int control_port;
	int data_port;
	int control_sock;
	int data_sock;
	int MAX_EVENT_NUMER;
	char path[1000];//默认路径
private:
	void init_thread();
	void process_control();	//处理控制数据的线程函数
	static void* process_data(void*);	//处理传输数据的线程函数
	void changeDir(char*);	//control
	void checkUser();		//control
	void getDir();			//data
	void getFileList(char*);		//data
	void addFile(File_struct*);	//data
	void delFile(char*);	//data
	void exit_func();		//data && control
};

/*

*/

#endif // SFTP
