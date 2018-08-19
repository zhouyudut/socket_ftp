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
	void process();//	��Ҫ��������
	void stop();
private:
	sem_t sem_a,sem_b;
	std::queue<Op_pool*> op_queue;
	std::queue<Re_pool*> re_queue;
private:
	bool auth;			//�û��Ƿ���֤
	int control_port;
	int data_port;
	int control_sock;
	int data_sock;
	int MAX_EVENT_NUMER;
	char path[1000];//Ĭ��·��
private:
	void init_thread();
	void process_control();	//����������ݵ��̺߳���
	static void* process_data(void*);	//���������ݵ��̺߳���
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
