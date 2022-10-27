#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"


//创建文件
int FileSystem::Func_CreateFile(vector<string> str)
{
	if (Func_CheckParaNum(str, 1) == -1)
	{
		return -1;
	}

	if (my_superblock.s_ninode == 0)//文件系统中文件/目录数已达上限
	{
		cout << "文件系统中文件/目录数已达上限，创建目录失败" << endl;
		return -1;
	}

	int ret = Func_AnalysePath(str[1]);
	if (ret != -1)//该路径已存在
	{
		cout << "已存在同名文件/目录，创建目录失败" << endl;
		return -1;
	}

	string new_file_name = Func_LastPath(str[1]);

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	int father_inode_num = Func_GetFatherInode(str[1]);
	Inode father_inode;
	Directory father_dir;
	fd.seekg(INODE_START * BLOCK_SIZE + father_inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&father_inode, sizeof(Inode));
	fd.seekg((BLOCK_START + father_inode.i_addr[0]) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&father_dir, sizeof(father_dir));



	int father_dir_insert = -1;
	for (int i = 0; i < MAX_SUBDIR; i++)
		if (father_dir.child_filedir[i].m_name[0] == '\0')
		{
			father_dir_insert = i;
			break;
		}
	if (father_dir_insert == -1)
	{
		cout << "当前目录下文件/目录数已达上限，创建目录失败" << endl;
		return -1;
	}

	Inode inode1;//根目录
	//根目录inode初始化
	inode1.i_mode = 0x1FF;					//目录，具体权限之后考虑
	inode1.i_nlink = 0;						//文件在不同目录树中的索引数量
	inode1.i_uid = user_now.id;						//文件所有者用户id
	//inode1.i_gid = user_now.groupid;						//文件所有者用户组id
	inode1.i_size = 0;			//文件大小，普通文件起始为0
	//inode1.i_addr[0] = Func_AllocateBlock();	//暂时不需要分配block块

	inode1.i_no = my_superblock.s_inode[--my_superblock.s_ninode];//栈顶inode序号，top操作
	//my_superblock.s_ninode--;//等价出栈，pop操作
	inode1.i_atime = time(NULL);			//最后访问时间
	inode1.i_mtime = time(NULL);			//最后修改时间

	//inode放入inode区对应位置中
	fd.seekg(INODE_START * BLOCK_SIZE + inode1.i_no * sizeof(Inode), ios::beg);
	fd.write((char*)&inode1, sizeof(Inode));



	//更新father目录
	strcpy(father_dir.child_filedir[father_dir_insert].m_name, new_file_name.c_str());
	father_dir.child_filedir[father_dir_insert].n_ino = inode1.i_no;

	if (now_dir.d_inode == father_inode_num)//记得更新当前所在目录(如果相同)
	{
		memcpy(&now_dir, &father_dir, sizeof(Directory));
	}
	fd.seekg((BLOCK_START + father_inode.i_addr[0]) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&father_dir, sizeof(father_dir));

	fd.close();//关文件

	Func_RenewDisk();//不要写在上面打开的文件里面，否则造成重复关闭

	cout << "创建新文件成功" << endl;
	return 1;
}


//打开文件：创建文件的打开结构
int FileSystem::Func_OpenFile(vector<string> str)
{
	if (Func_CheckParaNum(str, 1) == -1)
	{
		return -1;
	}
	if (inode_open_num == MAX_OPEN_FILE_NUM)//文件系统中打开文件数已达上限
	{
		cout << "文件系统中打开文件数已达上限，打开文件失败" << endl;
		return -1;
	}

	int open_inode_num = Func_AnalysePath(str[1]);
	if (open_inode_num == -1)//该路径不存在
	{
		cout << "路径不存在，打开文件失败" << endl;
		return -1;
	}

	for (int i = 0; i < inode_open_num; i++)
	{
		if (open_inode_num == my_inode_map[i].i_no)
		{
			cout << "该文件已打开，可直接使用" << endl;
			return -1;
		}
	}

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	//维护内存中file_inode
	fd.seekg(INODE_START * BLOCK_SIZE + open_inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&my_inode_map[inode_open_num], sizeof(Inode));
	//维护内存中my_file_map
	my_file_map[inode_open_num].inode_num = open_inode_num;
	my_file_map[inode_open_num].offset = 0;
	my_file_map[inode_open_num].openuser_id = user_now.id;
	my_file_map[inode_open_num].file_name = str[1];
	my_file_map[inode_open_num].user_name = user_now.name;
	//维护内存中inode_open_num
	inode_open_num++;

	fd.close();//关文件
	Func_RenewDisk();//同步到磁盘
	return 1;
}


/*对文件读写操作的总体思路是：读写还是直接对磁盘block操作，内存中不存储block相关的东西
但是文件管理相关的inode和打开结构还是在内存中。
相当于每一次开一个新的cmd都是对内存重新开始，先前打开的文件、文件指针等丢失，但是修改保留*/

//关闭文件：清除文件的打开结构
int FileSystem::Func_CloseFile(vector<string> str)
{
	if (Func_CheckParaNum(str, 1) == -1)
	{
		return -1;
	}

	int close_inode_num = Func_AnalysePath(str[1]);
	if (close_inode_num == -1)//该路径不存在
	{
		cout << "路径不存在，关闭文件失败" << endl;
		return -1;
	}

	int now_pos = -1;//当前文件的下标

	for (int i = 0; i < inode_open_num; i++)
	{
		if (close_inode_num == my_inode_map[i].i_no)
		{
			now_pos = i;
			break;
		}
	}

	//调整位置，维护两个数组
	if (now_pos == -1)
	{
		cout << "该文件未打开，关闭文件失败" << endl;
		return -1;
	}

	//删除now_pos位置，将最后一位移到此处
	memcpy(&my_inode_map[now_pos], &my_inode_map[inode_open_num - 1], sizeof(Inode));
	memcpy(&my_file_map[now_pos], &my_file_map[inode_open_num - 1], sizeof(File));

	//只需要维护内存中inode_open_num即可，不需要清除
	inode_open_num--;
	
	return 1;
}


//查看文件
int FileSystem::Func_CatFile(vector<string> str)
{
	if (Func_CheckParaNum(str, 1) == -1)
	{
		return -1;
	}

	int cat_inode_num = Func_AnalysePath(str[1]);
	if (cat_inode_num == -1)//该路径不存在
	{
		cout << "路径不存在，打开文件失败" << endl;
		return -1;
	}
	if (Func_AuthCheck(cat_inode_num, "read") == -1)
	{
		cout << "没有对应权限，打开文件失败" << endl;
		return -1;
	}

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + cat_inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));

	if ((inode.i_mode & Inode::INODEMODE::DIRECTORY) == Inode::INODEMODE::DIRECTORY)//目录，递归删除所有内容(记得最后删除自己)
	{
		cout << "路径为目录，打开文件失败" << endl;
		fd.close();//关文件
		return -1;
	}


	char buf[BLOCK_SIZE + 1];//打印缓冲区
	buf[BLOCK_SIZE] = '\0';
	//根据文件的size计算block_sum
	int block_sum;//该文件占用的数据区block数
	if (inode.i_size == 0)
		block_sum = 0;
	else
		block_sum = (inode.i_size - 1) / BLOCK_SIZE + 1;

	for (int i = 0; i < block_sum; i++)
	{
		int block_pos = Func_AddrMapping(cat_inode_num, i);
		fd.seekg((BLOCK_START + block_pos) * BLOCK_SIZE, ios::beg);
		fd.read((char*)buf, BLOCK_SIZE);
		if (i == block_sum - 1)
			buf[(inode.i_size - 1) % BLOCK_SIZE + 1] = '\0';
		cout << buf;

	}

	cout << endl;
	fd.close();//关文件
	return 1;
}


//读文件——指定字节大小
int FileSystem::Func_ReadFile(vector<string> str)
{
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}

	int cat_inode_num = Func_AnalysePath(str[1]);
	if (cat_inode_num == -1)//该路径不存在
	{
		cout << "路径不存在，打开文件失败" << endl;
		return -1;
	}

	if (Func_AuthCheck(cat_inode_num, "read") == -1)
	{
		cout << "没有对应权限，打开文件失败" << endl;
		return -1;
	}

	int read_byte = atoi(str[2].c_str());

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + cat_inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));

	if ((inode.i_mode & Inode::INODEMODE::DIRECTORY) == Inode::INODEMODE::DIRECTORY)//目录，递归删除所有内容(记得最后删除自己)
	{
		cout << "路径为目录，打开文件失败" << endl;
		fd.close();//关文件
		return -1;
	}
	int open_pos = -1;
	for (int i = 0; i < inode_open_num; i++)
	{
		if (cat_inode_num == my_inode_map[i].i_no)
		{
			open_pos = i;
			break;
		}
	}
	if (open_pos == -1)
	{
		cout << "该文件未打开，不可修改文件指针位置" << endl;
		return -1;
	}


	read_byte = min(read_byte, (inode.i_size - my_file_map[open_pos].offset));//若超过最大限度，则读到最后即可
	if (read_byte == 0)
	{
		cout << endl;
		fd.close();//关文件
		return 1;
	}

	int start_block = my_file_map[open_pos].offset / BLOCK_SIZE;
	int end_block = (my_file_map[open_pos].offset + read_byte - 1) / BLOCK_SIZE;
	char buf[BLOCK_SIZE + 1];//打印缓冲区
	buf[BLOCK_SIZE] = '\0';

	//[start_pos,end_pos)
	int start_pos = my_file_map[open_pos].offset;
	int end_pos = my_file_map[open_pos].offset + read_byte;

	for (int i = start_block; i <= end_block; i++)
	{
		int block_pos = Func_AddrMapping(cat_inode_num, i);
		fd.seekg((BLOCK_START + block_pos) * BLOCK_SIZE, ios::beg);
		fd.read((char*)buf, BLOCK_SIZE);

		char read_buf[BLOCK_SIZE + 1];//打印缓冲区
		memset(read_buf, 0, BLOCK_SIZE + 1);
		int this_start = start_pos % BLOCK_SIZE;
		int this_end;
		if (i == end_block)
		{
			this_end = (end_pos - 1) % BLOCK_SIZE;
			memcpy(read_buf, &buf[this_start], (this_end - this_start + 1));
		}
		else
		{
			this_end = BLOCK_SIZE - 1;
			memcpy(read_buf, &buf[this_start], (this_end - this_start + 1));
		}
		start_pos = i * BLOCK_SIZE;
		cout << read_buf;
	}
	cout << endl;
	my_file_map[open_pos].offset += read_byte;
	fd.close();//关文件
	return 1;
}





//修改文件指针位置
int FileSystem::Func_LseekFile(vector<string> str)
{
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}

	int new_seek_pos = atoi(str[2].c_str());

	int lseek_inode_num = Func_AnalysePath(str[1]);
	if (lseek_inode_num == -1)//该路径不存在
	{
		cout << "路径不存在，打开文件失败" << endl;
		return -1;
	}




	int open_pos = -1;
	for (int i = 0; i < inode_open_num; i++)
	{
		if (lseek_inode_num == my_inode_map[i].i_no)
		{
			open_pos = i;
			break;
		}
	}
	if (open_pos == -1)
	{
		cout << "该文件未打开，不可修改文件指针位置" << endl;
		return -1;
	}
	if ((my_inode_map[open_pos].i_mode & Inode::INODEMODE::DIRECTORY) == Inode::INODEMODE::DIRECTORY)//目录，递归删除所有内容(记得最后删除自己)
	{
		cout << "路径为目录，打开文件失败" << endl;
		return -1;
	}
	if (new_seek_pos<0 || new_seek_pos>my_inode_map[open_pos].i_size)
	{
		cout << "指针超出文件大小范围" << endl;
		return -1;
	}

	my_file_map[open_pos].offset = new_seek_pos;
	return 1;
}



//写文件
int FileSystem::Func_WriteFile(vector<string> str)
{
	/*if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}*/
	int keep_offset = 0;
	int file_myoffset;
	if (str.size() == 4 && str[3] == "myin")//外部导入，保留文件指针
	{
		keep_offset = 1;
	}
	else if(str.size() == 3)
	{
		;
	}
	else
	{
		cout << "参数有误，请重新输入" << endl;
		return -1;
	}

	int cat_inode_num = Func_AnalysePath(str[1]);
	if (cat_inode_num == -1)//该路径不存在
	{
		cout << "路径不存在，打开文件失败" << endl;
		return -1;
	}
	if (Func_AuthCheck(cat_inode_num, "write") == -1)
	{
		cout << "没有对应权限，打开文件失败" << endl;
		return -1;
	}

	int open_pos = -1;
	for (int i = 0; i < inode_open_num; i++)
	{
		if (cat_inode_num == my_inode_map[i].i_no)
		{
			open_pos = i;
			break;
		}
	}
	if (open_pos == -1)
	{
		cout << "该文件未打开，不可修改" << endl;
		return -1;
	}
	if ((my_inode_map[open_pos].i_mode & Inode::INODEMODE::DIRECTORY) == Inode::INODEMODE::DIRECTORY)//目录
	{
		cout << "路径为目录，写入文件失败" << endl;
		return -1;
	}

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Inode* write_inode = &(my_inode_map[open_pos]);
	File* write_file = &(my_file_map[open_pos]);

	if (keep_offset==1)
	{
		file_myoffset = write_file->offset;
		write_file->offset = 0;
	}

	//str[2].c_str();

	/*先根据需求分配盘块
	先根据当前文件指针和str[2].size确定需要涉及到的block块
	起始块和末尾块做特殊处理（先保存内容再更新再保存）*/
	int old_size = write_inode->i_size;
	int new_size = max((unsigned int)(write_inode->i_size), write_file->offset + str[2].size());
	int start_block;
	int end_block;
	/*if (write_file->offset == 0)
		start_block = 0;
	else
		start_block = (write_file->offset - 1) / BLOCK_SIZE + 1;
	if (write_file->offset + str[2].size() == 0)
		end_block = 0;
	else
		end_block = (write_file->offset + str[2].size() - 1) / BLOCK_SIZE + 1;*/

	start_block = (write_file->offset) / BLOCK_SIZE;
	end_block = (write_file->offset + str[2].size()) / BLOCK_SIZE;


	int ret = Func_OpenedFileAddBlock(open_pos, new_size);//扩充文件block成功，直接转化查找即可
	if (ret == -1)
	{
		write_inode->i_size = old_size;
		cout << "大小超出上限，添加失败" << endl;
		fd.close();//关文件
		Func_RenewDisk();//同步到磁盘
		return 1;
	}
	Func_SaveFileInode();//存储内存中的打开file相关inode

	//Func_AddrMapping()

	
	int str_pos_start = 0;//
	int last_length = str[2].size();//剩余没有copy的长度
	
	int buf_start = write_file->offset % BLOCK_SIZE;//memcpy到buf中的起始位置
	int buf_len = min(512 - buf_start, last_length);//本次memcpy到buf的长度
	//以block为单位顺次处理，特殊处理起始位置
	for (int logic_addr = start_block; logic_addr <= end_block; logic_addr++)
	{
		//先读入
		char buf[BLOCK_SIZE];
		int block_pos = Func_AddrMapping(cat_inode_num, logic_addr);
		fd.seekg((BLOCK_START + block_pos) * BLOCK_SIZE, ios::beg);
		fd.read((char*)buf, BLOCK_SIZE);

		//拷贝并维护
		memcpy(&buf[buf_start], &str[2].c_str()[str_pos_start], buf_len);

		fd.seekg((BLOCK_START + block_pos) * BLOCK_SIZE, ios::beg);
		fd.write((char*)buf, BLOCK_SIZE);

		str_pos_start += buf_len;
		last_length -= buf_len;
		buf_start = 0;
		buf_len = min(512 - buf_start, last_length);
	}

	write_file->offset += str[2].size();
	if (keep_offset == 1)
	{
		write_file->offset = file_myoffset;
	}

	fd.close();//关文件
	Func_RenewDisk();//同步到磁盘
	return 1;
}


//从本机系统中导入文件
int FileSystem::Func_WriteFileFromMine(vector<string> str)
{
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}

	ifstream ifs(str[2].c_str(), ios::in | ios::binary);
	string content((istreambuf_iterator<char>(ifs)),(istreambuf_iterator<char>()));
	vector<string> str1(4);
	str1[0] = "write";
	str1[1] = str[1];
	str1[2] = content;
	str1[3] = "myin";

	Func_WriteFile(str1);
	ifs.close();



	return 1;
}



int FileSystem::Func_WriteFileToMine(vector<string> str)
{
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}

	ofstream ifs(str[2].c_str(), ios::out| ios::trunc| ios::binary);


	int cat_inode_num = Func_AnalysePath(str[1]);
	if (cat_inode_num == -1)//该路径不存在
	{
		cout << "路径不存在，打开文件失败" << endl;
		ifs.close();
		return -1;
	}
	if (Func_AuthCheck(cat_inode_num, "read") == -1)
	{
		cout << "没有对应权限，打开文件失败" << endl;
		ifs.close();
		return -1;
	}

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + cat_inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));

	if ((inode.i_mode & Inode::INODEMODE::DIRECTORY) == Inode::INODEMODE::DIRECTORY)//目录，递归删除所有内容(记得最后删除自己)
	{
		cout << "路径为目录，打开文件失败" << endl;
		ifs.close();
		fd.close();//关文件
		return -1;
	}


	char buf[BLOCK_SIZE];//打印缓冲区
	
	//buf[BLOCK_SIZE] = '\0';
	//根据文件的size计算block_sum
	int block_sum;//该文件占用的数据区block数
	if (inode.i_size == 0)
		block_sum = 0;
	else
		block_sum = (inode.i_size - 1) / BLOCK_SIZE + 1;

	for (int i = 0; i < block_sum; i++)
	{
		int block_pos = Func_AddrMapping(cat_inode_num, i);
		fd.seekg((BLOCK_START + block_pos) * BLOCK_SIZE, ios::beg);
		fd.read((char*)buf, BLOCK_SIZE);
		int slen = BLOCK_SIZE;
		if (i == block_sum - 1)
		{
			slen = (inode.i_size - 1) % BLOCK_SIZE + 1;
		}

		ifs.write(buf, slen);
		ifs.flush();

	}


	fd.close();//关文件
	ifs.close();

	return 1;
}





int FileSystem::Func_OpenFileState(vector<string> str)
{
	if (Func_CheckParaNum(str, 0) == -1)
	{
		return -1;
	}
	cout << setw(16) << left << "file_name";
	cout << setw(16) << left << "user_name";
	cout << setw(16) << left << "offset";
	cout << endl;

	for (int i = 0; i < inode_open_num; i++)
	{
		//打印名字
		cout << setw(16) << left << my_file_map[i].file_name;
		cout << setw(16) << left << my_file_map[i].user_name;
		cout << setw(16) << left << my_file_map[i].offset;
		cout << right;
		cout << endl;
	}
	return 1;
}














