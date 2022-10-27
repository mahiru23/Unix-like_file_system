#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <string.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <map>
#include <sstream>
#include <algorithm>
#include "parameter.h"
using namespace std;

/*一组状态宏定义，考虑枚举类 or 。。。*/
#define TRUE		1
#define FALSE		0
#define OK		1
#define ERROR		0
#define INFEASIBLE	-1
typedef int Status;


//成组链接的block块
class BlockGroup {
public:
	int s_nfree;
	int s_free[BLOCK_FREE_NUM];
};


/*本项目尝试构建的文件系统是：
superblock——inode——文件数据区的三段存储层次结构*/
/*superblock：占用0、1两个block*/
class SuperBlock {
public:
	/*inode为文件总数的限制，处于简化考虑，不对其使用成组链接法，写死为100*/
	int disk_block_size;//磁盘的block总数
	int s_isize;			//inode占用的block盘块数
	int s_ninode;			//直接管理的空闲inode数
	int s_inode[INODE_NUM];	//管理的inode栈，里面存编号

	/*实际block：数量较多，对其使用成组链接法*/
	int s_fsize;				//数据区使用的block总数
	int s_nfree;				//直接管理的空闲盘块数
	int s_free[BLOCK_FREE_NUM];	//直接管理的空闲盘块索引表

	/*其他相关项*/
	int s_fmod;		//内存中superblock被修改，写回
	int s_ronly;	//文件系统只能读出
	time_t s_time;	//最近一次更新时间
	int padding[66];//填充，使superblock大小为1024
};

/*inode，占用10个block
单个inode结构大小为64，1个block能放8个inode，系统共80个可用inode，即可创建文件上限为80*/
/*一个inode代表一个文件*/
/*inode目前考虑在superblock中使用位示图进行管理*/

/*文件的打开结构：？？？？此处只记录某个文件在系统中的打开数量，切换用户保留 or 关闭？？？（目前考虑写在硬盘中，每次打开cmd重置）

*/
class Inode {
public:
	/*10位有效位，drwxrwxrwx*/
	enum INODEMODE {
		OTHER_x = 0x1,
		OTHER_w = 0x2,
		OTHER_r = 0x4,
		GROUP_x = 0x8,
		GROUP_w = 0x10,
		GROUP_r = 0x20,
		OWNER_x = 0x40,
		OWNER_w = 0x80,
		OWNER_r = 0x100,
		DIRECTORY = 0x200
	};

	unsigned int i_mode;	//enum INODEMODE中定义的文件权限(暂定为10位)
	int i_nlink;			//文件在不同目录树中的索引数量
	int i_uid;				//文件所有者用户id
	char usergroup[12];		//用户组名，每个用户可以有很多个用户组，但只有一个主用户组
	int i_size;				//文件大小（字节数）
	int i_addr[10];			//逻辑块号和物理块号转换索引表
	int i_no;				//文件的inode序号（便于内存中inode写回磁盘查找位置）
	time_t  i_atime;		//最后访问时间
	time_t  i_mtime;		//最后修改时间
	int padding[9];			//填充，使inode节点大小为128
};

/*是文件，block数据区中存的是文件内容
是目录，block数据区中存的是directory结构 */

/*文件的打开结构，一个file对应着某个用户的一次打开
对于多用户打开多个文件情况，使用锁机制来控制
注意，一个用户最多只能打开一个相同文件。在打开文件之前首先查询
使用一个内存中的文件管理表来进行管理
单次打开cmd相当于开机，关闭cmd=关机*/
class File {
public:
	string file_name;		//文件名
	string user_name;		//用户名
	int inode_num;			//文件对应的inode号
	int offset;				//文件指针（偏移量位置），每次打开后被重置
	int openuser_id;		//打开该文件的用户id
};


/*注意，需要写在block中的内容不能使用string，因为大小不确定。
向block中写东西是非常底层的内存操作，而string是一个很高级语言的封装类，两者显然会产生冲突*/
/*目录项：可能是目录，也可能是文件，取决于inode中mode设置
DIRSIZ=28，DirectoryEntry=32*/
class DirectoryEntry {
public:
	int n_ino;			//目录项中Inode编号部分
	char m_name[MAX_NAMELEN];//目录项中文件名部分
};

/*目录，存目录项，有上限（暂时未实现实时扩充）
MAX_SUBDIR=16，一个Directory为32*16=512字节，占一个block
存储是摊平的，目录是树形的，因此需要对两者做一个转换*/
class Directory {
public:
	DirectoryEntry child_filedir[MAX_SUBDIR];	//子目录/文件
	char m_name[MAX_NAMELEN];						//改进，在目录中写自己的文件名
	int d_inode;								//填充
};

/*用户，数据存在一个单独的文件里*/
/*5个属性，自由设置*/
class UserInfo {


public:
	int id;				//用户序号，0为root
	char name[12];		//用户名
	char usergroup[12];	//用户组名，每个用户可以有很多个用户组，但只有一个主用户组
	char password[16];	//注意是md5算法加密后的用户密码，private封装,MD5最长为128bit=16byte

	const char * Func_Encryption(const char password1[12]);//存储密码：将明文密码加密为密文

};

/*系统中用户的所有信息，存在某个block中，不在内存中常态存在*/
class Users {
public:
	int user_num;				//系统存储的用户总数（上限为10，一个block中）
	int user_now;				//当前用户累计数（分配给下一个用户的uid号）
	UserInfo user[MAX_USER_NUM];//存储用户信息
	int padding[16];			//填充字节
};


/*系统整体，注意使用单例模式！！！！！！*/
// 局部静态变量实现的懒汉式单例，C++11后线程安全
/*C++11规定了local static在多线程条件下的初始化行为，要求编译器保证了内部静态变量的线程安全性。
在C++11标准下，《Effective C++》提出了一种更优雅的单例模式实现，使用函数内的 local static 对象。
这样，只有当第一次访问getInstance()方法时才创建实例。这种方法也被称为Meyers' Singleton。*/
class FileSystem
{
private:
	FileSystem() { };
	~FileSystem() { };
	FileSystem(const FileSystem&);
	FileSystem& operator=(const FileSystem&);

	/*以下为文件系统自定义内容，与单例无关*/
	UserInfo user_now;//当前系统中登录的用户

	/*指令，实现指令集的目的是为了实现指令增减的自动化*/
	/*通过函数指针的方式，绑定一组函数与string的对应关系*/
	unordered_map<string, int(FileSystem::*)(vector<string>)> inst;	//指令集
	SuperBlock my_superblock;										//内存superblock

	/*打开文件相关*/
	int inode_open_num;						//已经打开的inode数(只有文件)
	Inode my_inode_map[MAX_OPEN_FILE_NUM];	//内存inode数组
	File my_file_map[MAX_OPEN_FILE_NUM];	//打开的文件结构

	/*路径相关*/
	Directory now_dir;		//当前位于的direction

public:

	/*一系列处理函数，考虑用enum+函数指针？？？*/
	/*对于每一个操作来说，都需要当前用户拥有对应的权限
	这种权限有时是写死的（如root专享），有时是取决于对应文件操作的情况（文件权限与当前用户）*/
	/*要针对对应报错情况，给出一组宏定义（最好是枚举类），这样，直接返回具有对应含义的报错符号即可（并同时维护errno），显著提高代码可读性及系统稳定性（对用户来说）*/

	/*root权限才能操作的一组函数，用于用户和用户组的维护*/
	int Func_CheckUser(string name);						//从用户文件中检查用户是否存在
	int Func_UserToken(string name, string passwd);			//登录时检查用户名和密码是否匹配
	int Func_AddUser(vector<string> str);					//新建用户
	int Func_DelUser(vector<string> str);					//删除用户
	int Func_AddUserToGroup(vector<string> str);			//添加用户到用户组
	int Func_DelUserFromGroup(vector<string> str);			//从用户组中删除用户

	/*用户信息及切换一组函数，多用户注意考虑线程安全*/
	int Func_Whoami(vector<string> str);				//打印当前用户信息
	int Func_ShowAllUser(vector<string> str);			//打印系统中所有用户信息
	int Func_Su(vector<string> str);					//切换用户，root不需要密码，其他需要
	int Func_UserExit(vector<string> str);				//退出当前用户（回到初始无用户界面）
	int Func_UpdateUserPassword(vector<string> str);	//修改用户密码，root可修改所有用户，普通用户只能修改自己


	/*目录相关操作*/
	int Func_Mkdir(vector<string> str);						//创建目录
	int Func_Rm(vector<string> str);						//删除目录（及其中所有内容）或文件
	int Func_Rm_plus(int inode_num, int father_inode_num);	//删除当前inode对应的位置
	int Func_Ls(vector<string> str);						//ls操作，列出当前目录下所有文件（包括修改时间具体信息）
	int Func_Cd(vector<string> str);						//cd，进入某个路径
	int Func_Chmod(vector<string> str);						//修改路径或者文件的权限
	int Func_Help(vector<string> str);						//打印全局或对应指令的help信息

	/*具体的文件操作，创建、删除、开闭等*/
	int Func_CreateFile(vector<string> str);	//创建新文件
	int Func_OpenFile(vector<string> str);		//打开文件
	int Func_CloseFile(vector<string> str);		//关闭文件
	int Func_CatFile(vector<string> str);		//显示文件中所有内容（不需要打开，不依赖文件指针）

	/*对某个文件的修改操作（和文件指针相关）*/
	int Func_ReadFile(vector<string> str);			//读某个文件
	int Func_WriteFile(vector<string> str);			//写某个文件
	int Func_LseekFile(vector<string> str);			//移动文件指针，根据当前文件的文件指针进行读写
	int Func_WriteFileFromMine(vector<string> str);	//从本机系统中导入文件
	int Func_WriteFileToMine(vector<string> str);	//导出文件到本机系统
	int Func_OpenFileState(vector<string> str);		//打开文件状态

	/*系统相关函数，例如初始化或者格式化，大部分不依赖用户身份*/
	int Func_CreateNewFileSystem();		//从零开始创建文件系统
	int Func_MountExistedFileSystem();	//加载已有文件系统
	int Func_CheckExistedFileSystem();	//检测已有文件系统是否损坏，是则选择是否新建
	int Func_FileSystemExit();			//退出当前文件系统

	/*登录与shell相关，系统基础组成部分*/
	int Func_LoginLoop();									//循环登录
	int Func_IsUserExist(string user_name, string password);//查找该用户是否存在
	int Func_ShellLoop();									//进入shell，之后进行shell读取循环
	int Func_InitInstructions();							//初始化指令集（对应填充）
	int Func_CheckParaNum(vector<string> str, int para_num);//统一用于检测参数个数是否符合预期

	/*对于磁盘和内存的同步更新，凡是修改性更新都要考虑执行*/
	int Func_RenewMemory();		//将磁盘中对应位置内容读入内存中，初始化时读入磁盘中superblock和根目录
	int Func_RenewDisk();		//将内存中对应位置内容读入磁盘中，改内存后执行
	int Func_SaveFileInode();	//存储内存中的打开file相关inode

	/*对映射、转换等内容的设置*/
	int Func_AddrMapping(int inode, int logic_addr);	//根据inode中addr表，将逻辑盘块号转换为物理盘块号（处理文件时也按照逻辑块进行处理）
	int Func_AnalysePath(string str);					//分解路径，查找路径对应的inode序号并返回
	bool Func_CheckDirExist(int inode, string str);		//遍历判断该inode对应dir下是否存在同名文件/文件夹

	/*对superblock中数据块结构的维护*/
	int Func_ReleaseBlock(int block_num);	//对数据块block统一释放
	int Func_AllocateBlock();				//对数据块block统一分配

	/*路径相关*/
	int Func_GetFatherInode(string str);					//获取父亲目录的inode号
	int Func_OpenedFileAddBlock(int now_pos, int new_size);	//对内存中已经打开的文件及inode进行操作
	string Func_LastPath(string str);						//获取path中最后一个可用位置

	/*权限相关*/
	int Func_AuthCheck(int file_inode, string str);		//对当前用户是否能够进行操作的权限进行验证
	const char* Func_FromUseridGetGroup(int user_id);	//根据uid获取groupname

	static FileSystem& getInstance()//单例模式唯一外界可用——获取实例
	{
		static FileSystem instance;
		return instance;
	}
};









