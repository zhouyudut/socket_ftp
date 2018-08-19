#include <queue>
#include "type.h"
#include <semaphore.h>
class ftp_client
{
public:
	int control_sock;
	int data_sock;
public:
	ftp_client();
	~ftp_client();
	bool init_client(char* ip,int control_port,int data_port);
	void run();
private:
	pthread_t pid;
	bool pthread_status;
private:
	Client_op_control send_struct;
	Server_re_control recv_struct;
	char local_path[200];
	char remote_path[200];
	sem_t sem_a,sem_b;
	std::queue<Op_pool*> op_queue;
	std::queue<Re_pool*> re_queue;

private:
    void run_control();
    static void* run_data(void* arg);
private:
	void exit_thread();
	void exit_sock();
	void read_stdin();
	void work_process();	//TODO
	void recv_result();		//TODO
private:
	void sendFile(char*);
	void delFile(char*);
	void GetFileList(char*);//???//需不需要传参
	void GetDir();		//
	void my_recv(int sock,char* buf,int max_lenth);
	void my_sig_recv(char* buf,int lenth);
};
