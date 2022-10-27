#pragma once
# include <iostream>
# include <fstream>
# include <string>
# include <time.h>
# include <iomanip>
#include<bitset>
using namespace std;


/*本头文件用来存储宏定义*/
static const char* MYDISK_NAME = "FileDisk.img";
/*文件和目录的权限对应位*/
static const char* PRIVILEGE = "drwxrwxrwx";



/*Superblock各项数据定义*/
//单个block的size大小（宏定义可以自适应）
#define BLOCK_SIZE 512 
//inode的总数/管理数
#define INODE_NUM 80 
//block的直接管理数
#define BLOCK_FREE_NUM 100

//superblock段的block数
#define SUPERBLOCK_BLOCK_NUM 2
//inode段占用的block数
#define INODE_BLOCK_NUM 20
//block数据段的block数（最大文件上限大约在70000左右）
#define BLOCK_NUM 100000

//成组链接法，每组的block
#define BLOCK_GROUP_NUM 100

//三个数据段的起始位置
#define SUPERBLOCK_START 0
#define INODE_START 2
#define BLOCK_START 22

//其他部分定义
#define SUPERBLOCK_WRITEBACK 1
#define SUPERBLOCK_READWRITE 0
#define SUPERBLOCK_READONLY 1


//目录结构相关定义
//目录和文件的最长命名
#define MAX_NAMELEN 28
//每个目录的最多的子目录/文件数
#define MAX_SUBDIR 15
//目录的大小
#define DIRECTORY_SIZE 512 
//开机加载根目录位置
#define ROOT_DIRECTORY_LOAD 99

//逻辑块到物理块的索引转换
//直接映射上限
#define ZERO_INDEX 6
//一级索引映射上限
#define ONE_INDEX 6+128*2
//二级索引映射上限
#define TWO_INDEX 6+128*2+128*128*2

//各级索引中，每个索引块能够存储的内容大小
#define ZERO_INDEX_NUM 1
#define ONE_INDEX_NUM 128
#define TWO_INDEX_NUM 128*128

//block索引块中存储形式为int[128]
#define BLOCK_INT_NUM 128

//最多同时打开文件数
#define MAX_OPEN_FILE_NUM 20


//系统最大用户数
#define MAX_USER_NUM 10


//系统用户文件存储盘块（只有一个，初始化确定）
#define USER_INFO_BLOCK 98

//用于退出当前shell的定义
#define EXIT_SHELL -213704







