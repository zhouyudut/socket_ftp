/*
	checkuser		6011

	getdir			7011
	chagedir		7022
	makedir			7033

	getfilelist		8011
	addfile			8022
	delfile 		8033
	sendfile		8044
*/
#ifndef TYPE_H
#define TYPE_H

typedef struct
{
	int CLIENT_OP_CODE;
	char str[200];
	char other[200];
	char test_msg[200];//纯属测试用
	unsigned long file_size;
}Client_op_control;

typedef struct
{
	int SERVER_OP_CODE;
	int SERVER_RE_CODE;
	char msg[200];
}Server_re_control;

typedef enum{
	CHECKUSER,

	GETFILE,
	ADDFILE,
	DELFILE,
	RENAMEFILE,

	FILELIST,

	GETDIR,/*获取目录路径*/
	ADDDIR,
	DELDIR,
	CHANGEDIR,
	RENAMEDIR
}OP_STATUS;

typedef enum{
	SUCCESS=0,
	FAILED=1,
	EXIT_NOW
	//....待补充的错误

}RE_STATUS;

typedef struct{
	int OP_CODE;
	char str[200];
//	bool exist;
	bool exited;
}Op_pool;

typedef struct{
	RE_STATUS RE_CODE;
	char message[200];
}Re_pool;


//记录文件夹的信息，包括绝对路径名和所包含的文件或文件夹数
typedef struct {
	char name[1024];
	int num;
	bool isERROR;
}Dir_struct;

//记录文件或文件夹信息，包括相对路径名和文件大小
typedef struct{
	char name[512];
	int isDir;
	unsigned long fileSize;
	int id;
}File_struct;


#endif // TYPE_H
