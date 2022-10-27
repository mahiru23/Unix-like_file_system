#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"

/*组件库，存放开发过程中需要用到的工具函数*/

//Instructions构造函数，初始化指令集map
int FileSystem::Func_InitInstructions()
{
	inst["help"] = &FileSystem::Func_Help;//函数指针写法
	inst["mkdir"] = &FileSystem::Func_Mkdir;
	inst["ls"] = &FileSystem::Func_Ls;
	inst["chmod"] = &FileSystem::Func_Chmod;
	inst["cd"] = &FileSystem::Func_Cd;
	inst["rm"] = &FileSystem::Func_Rm;

	//文件操作
	inst["create"] = &FileSystem::Func_CreateFile;
	inst["open"] = &FileSystem::Func_OpenFile;
	inst["close"] = &FileSystem::Func_CloseFile;
	inst["cat"] = &FileSystem::Func_CatFile;
	inst["read"] = &FileSystem::Func_ReadFile;
	inst["write"] = &FileSystem::Func_WriteFile;
	inst["lseek"] = &FileSystem::Func_LseekFile;
	inst["writein"] = &FileSystem::Func_WriteFileFromMine;
	inst["writeout"] = &FileSystem::Func_WriteFileToMine;
	inst["opened"] = &FileSystem::Func_OpenFileState;

	//多用户相关操作
	inst["adduser"] = &FileSystem::Func_AddUser;
	inst["deluser"] = &FileSystem::Func_DelUser;
	inst["addusertogroup"] = &FileSystem::Func_AddUserToGroup;
	inst["deluserfromgroup"] = &FileSystem::Func_DelUserFromGroup;
	inst["whoami"] = &FileSystem::Func_Whoami;
	inst["showalluser"] = &FileSystem::Func_ShowAllUser;
	inst["su"] = &FileSystem::Func_Su;
	inst["exit"] = &FileSystem::Func_UserExit;
	inst["updatepassword"] = &FileSystem::Func_UpdateUserPassword;
	

	return 1;
}

//磁盘同步到内存
int FileSystem::Func_RenewMemory()
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	//更新my_superblock
	fd.seekg(SUPERBLOCK_START * BLOCK_SIZE, ios::beg);
	fd.read((char*)&my_superblock, sizeof(my_superblock));//转换成char*直接写入文件，需要读取时直接从文件中同样方式类型转换读到内存

	//更新当前的directory

	//更新数据块block
	fd.seekg((BLOCK_START + ROOT_DIRECTORY_LOAD) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&now_dir, sizeof(now_dir));

	fd.close();
	return 1;
}

//内存同步到磁盘
int FileSystem::Func_RenewDisk()
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	//更新my_superblock
	fd.seekg(SUPERBLOCK_START * BLOCK_SIZE, ios::beg);
	fd.write((char*)&my_superblock, sizeof(my_superblock));//转换成char*直接写入文件，需要读取时直接从文件中同样方式类型转换读到内存

	//更新当前的directory

	//查找inode对应的磁盘位置
	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + now_dir.d_inode * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));

	//更新修改和访问时间
	inode.i_mtime = time(NULL);
	inode.i_atime = time(NULL);

	//更新数据块block
	fd.seekg((BLOCK_START + inode.i_addr[0]) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&now_dir, sizeof(now_dir));

	//更新当前目录inode
	fd.seekg(INODE_START * BLOCK_SIZE + now_dir.d_inode * sizeof(Inode), ios::beg);
	fd.write((char*)&inode, sizeof(Inode));

	fd.close();
	return 1;
}

//存储内存中的打开file相关inode
int FileSystem::Func_SaveFileInode()
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	//将内存的打开文件inode写入磁盘
	for (int i = 0; i < inode_open_num; i++)
	{
		fd.seekg(INODE_START * BLOCK_SIZE + my_inode_map[i].i_no * sizeof(Inode), ios::beg);
		fd.write((char*)&my_inode_map[i], sizeof(Inode));
	}


	fd.close();
	return 1;
}

//拆分路径（返回-1代表路径不存在）
int FileSystem::Func_AnalysePath(string str)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	vector <string> path_list;//以vector形式分解的路径
	int pos;
	string sub_path = "";
	int start_inode;//起始位置的inode
	if (str[0] == '/')//绝对路径（从根目录开始）
	{
		//path_list.push_back("~");//先插入根目录
		start_inode = 0;//根目录inode
		for (pos = 1; pos <= str.size(); pos++)
		{
			if (pos == str.size())
			{
				if (sub_path == "")
					break;
				path_list.push_back(sub_path);
			}
			if (str[pos] == '/')//分界符
			{
				path_list.push_back(sub_path);
				sub_path = "";
			}
			else
			{
				sub_path += str[pos];
			}
		}
	}
	else//相对路径（从当前开始）
	{
		//path_list.push_back(now_dir.m_name);//先插入当前目录
		start_inode = now_dir.d_inode;//当前路径
		for (pos = 0; pos <= str.size(); pos++)
		{
			if (pos == str.size())
			{
				if (sub_path == "")
					break;
				path_list.push_back(sub_path);
			}
			if (str[pos] == '/')//分界符
			{
				path_list.push_back(sub_path);
				sub_path = "";
			}
			else
			{
				sub_path += str[pos];
			}
		}
	}

	/*路径分解完成后的处理相同（根据vector顺次找path）*/
	for (int i = 0; i < path_list.size(); i++)
	{
		//根据name找inode号
		Inode inode;
		Directory dir;
		//根据当前start_inode号查对应inode结构
		fd.seekg(INODE_START * BLOCK_SIZE + start_inode * sizeof(Inode), ios::beg);
		fd.read((char*)&inode, sizeof(Inode));
		//根据inode结构查directory结构
		fd.seekg((BLOCK_START + inode.i_addr[0]) * BLOCK_SIZE, ios::beg);
		fd.read((char*)&dir, sizeof(dir));
		//根据directory结构查下一层是否有对应name
		int over = 0;
		for (int j = 0; j < MAX_SUBDIR; j++)
		{
			if (strcmp(dir.child_filedir[j].m_name, path_list[i].c_str()) == 0)//找到
			{
				start_inode = dir.child_filedir[j].n_ino;
				over = 1;
				break;
			}
		}
		if (over == 0)
		{
			fd.close();
			//cout << "路径不存在，重新输入！" << endl;
			return -1;
		}
	}
	fd.close();
	return start_inode;
}

//至于逻辑块号应当根据文件指针计算出来
//inode对应的文件，逻辑块映射为物理块
int FileSystem::Func_AddrMapping(int inode_num, int logic_addr)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);
	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));

	if (logic_addr >= 0 && logic_addr < ZERO_INDEX)//直接映射
	{
		fd.close();
		return inode.i_addr[logic_addr];
	}
	else if (logic_addr >= ZERO_INDEX && logic_addr < ONE_INDEX)//一级索引
	{
		int index0 = (logic_addr - ZERO_INDEX) / ONE_INDEX_NUM + 6;//6 or 7，零级索引
		int index1 = (logic_addr - ZERO_INDEX) % ONE_INDEX_NUM;//一级索引
		int block_num1 = inode.i_addr[index0];//一级索引block号
		int block1[BLOCK_INT_NUM];
		//查一级索引block位置
		fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
		fd.read((char*)&block1, sizeof(block1));

		fd.close();
		return block1[index1];
	}
	else if (logic_addr >= ONE_INDEX && logic_addr < TWO_INDEX)//二级索引
	{
		int index0 = (logic_addr - ONE_INDEX) / TWO_INDEX_NUM + 8;//8 or 9，零级索引
		int index1 = ((logic_addr - ONE_INDEX)% TWO_INDEX_NUM) / ONE_INDEX_NUM;//一级索引 0-127
		int index2 = ((logic_addr - ONE_INDEX) % TWO_INDEX_NUM) % ONE_INDEX_NUM;//二级索引 0-127
		int block_num1 = inode.i_addr[index0];//一级索引block号
		int block1[BLOCK_INT_NUM];
		//查一级索引block位置
		fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
		fd.read((char*)&block1, sizeof(block1));
		int block_num2 = block1[index1];
		//查二级索引block位置
		int block2[BLOCK_INT_NUM];
		fd.seekg((BLOCK_START + block_num2) * BLOCK_SIZE, ios::beg);
		fd.read((char*)&block2, sizeof(block2));

		fd.close();
		return block2[index2];
	}
	else//错误
	{
		cout << "映射转换错误，大小超出上限" << endl;
		fd.close();
		return -1;
	}
	return -1;
}


int FileSystem::Func_CheckParaNum(vector<string> str, int para_num)
{
	if (str.size() < para_num + 1)
	{
		cout << "参数过少，请重新输入" << endl;
		return -1;
	}
	if (str.size() > para_num + 1)
	{
		cout << "参数过多，请重新输入" << endl;
		return -1;
	}
	return 1;
}

//遍历判断该inode对应dir下是否存在同名文件/文件夹
bool FileSystem::Func_CheckDirExist(int inode, string str)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	int block_num = Func_AddrMapping(inode, 0);
	Directory dir;
	fd.seekg((BLOCK_START + block_num) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&dir, sizeof(dir));
	bool result = false;
	for (int i = 0; i < MAX_SUBDIR; i++)
	{
		if (dir.child_filedir[i].m_name[0] == '\0')
		{
			break;
		}
		if (strcmp(dir.child_filedir[i].m_name, str.c_str()) == 0)//找到
		{
			result = true;
			break;
		}

	}

	fd.close();
	return result;
}


//磁盘回收文件释放的块
int FileSystem::Func_ReleaseBlock(int block_num)
{
	if (my_superblock.s_fsize == 0)
	{
		cout << "已经达到磁盘大小下限" << endl;
		return -1;
	}

	my_superblock.s_fsize--;//维护当前磁盘占用情况

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	//此时可能的为1-100
	if (my_superblock.s_nfree == BLOCK_GROUP_NUM)//superblock中已经满了，新来的block做存储链接块
	{
		BlockGroup newBG;
		newBG.s_nfree = my_superblock.s_nfree;
		memcpy(newBG.s_free, my_superblock.s_free, BLOCK_FREE_NUM * sizeof(int));
		
		//将superblock中满的组写到磁盘中
		fd.seekg((BLOCK_START + block_num) * BLOCK_SIZE, ios::beg);
		fd.write((char*)&newBG, sizeof(BlockGroup));//读入成功

		//更新superblock
		my_superblock.s_nfree = 1;
		my_superblock.s_free[0] = block_num;
	}
	else
	{
		my_superblock.s_free[my_superblock.s_nfree] = block_num;
		my_superblock.s_nfree++;
	}

	fd.close();
	return 1;
}

//从磁盘中给superblock分配一个块
int FileSystem::Func_AllocateBlock()
{
	if (my_superblock.s_fsize== BLOCK_NUM)
	{
		cout << "已经达到磁盘大小上限" << endl;
		return -1;
	}

	my_superblock.s_fsize++;//维护当前磁盘占用情况
	
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);
	int result = my_superblock.s_free[--my_superblock.s_nfree];//目录设置一位索引

	/*当最后一个空闲块被分配，s_nfree的值为1（一组只有99个块能用，[0]连接下一组）
	s_nfree=0时，将链接组块也分配出去，但是在分配连接块前先将其中数据放入superblock中*/
	if (my_superblock.s_nfree == 0)//superblock此时直接管理空闲块为0，更新
	{
		int numx = my_superblock.s_free[0];//下一组空闲块位置
		my_superblock.s_nfree = 100;//？？
		//读入新一组block到superblock中
		fd.seekg((BLOCK_START + numx) * BLOCK_SIZE + sizeof(int), ios::beg);
		fd.read((char*)my_superblock.s_free, sizeof(int) * BLOCK_FREE_NUM);//读入成功
	}

	fd.close();
	return result;

}




int FileSystem::Func_GetFatherInode(string str)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	vector <string> path_list;//以vector形式分解的路径
	int pos;
	string sub_path = "";
	int start_inode;//起始位置的inode
	if (str[0] == '/')//绝对路径（从根目录开始）
	{
		//path_list.push_back("~");//先插入根目录
		start_inode = 0;//根目录inode
		for (pos = 1; pos <= str.size(); pos++)
		{
			if (pos == str.size())
			{
				if (sub_path == "")
					break;
				path_list.push_back(sub_path);
			}
			if (str[pos] == '/')//分界符
			{
				path_list.push_back(sub_path);
				sub_path = "";
			}
			else
			{
				sub_path += str[pos];
			}
		}
	}
	else//相对路径（从当前开始）
	{
		//path_list.push_back(now_dir.m_name);//先插入当前目录
		start_inode = now_dir.d_inode;//当前路径
		for (pos = 0; pos <= str.size(); pos++)
		{
			if (pos == str.size())
			{
				if (sub_path == "")
					break;
				path_list.push_back(sub_path);
			}
			if (str[pos] == '/')//分界符
			{
				path_list.push_back(sub_path);
				sub_path = "";
			}
			else
			{
				sub_path += str[pos];
			}
		}
	}

	/*路径分解完成后的处理相同（根据vector顺次找path）*/
	for (int i = 0; i < path_list.size()-1; i++)
	{
		//根据name找inode号
		Inode inode;
		Directory dir;
		//根据当前start_inode号查对应inode结构
		fd.seekg(INODE_START * BLOCK_SIZE + start_inode * sizeof(Inode), ios::beg);
		fd.read((char*)&inode, sizeof(Inode));
		//根据inode结构查directory结构
		fd.seekg((BLOCK_START + inode.i_addr[0]) * BLOCK_SIZE, ios::beg);
		fd.read((char*)&dir, sizeof(dir));
		//根据directory结构查下一层是否有对应name
		int over = 0;
		for (int j = 0; j < MAX_SUBDIR; j++)
		{
			if (strcmp(dir.child_filedir[j].m_name, path_list[i].c_str()) == 0)//找到
			{
				start_inode = dir.child_filedir[j].n_ino;
				over = 1;
				break;
			}
		}
		if (over == 0)
		{
			fd.close();
			//cout << "路径不存在，重新输入！" << endl;
			return -1;
		}
	}
	fd.close();
	return start_inode;
}

//对内存中已经打开的文件及inode进行操作addblock(操作的是内存中内容)
int FileSystem::Func_OpenedFileAddBlock(int now_pos, int new_size)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Inode* write_inode = &(my_inode_map[now_pos]);

	int old_block_sum;
	int new_block_sum;
	if (write_inode->i_size == 0)
		old_block_sum = 0;
	else
		old_block_sum = (write_inode->i_size - 1) / BLOCK_SIZE + 1;
	write_inode->i_size = new_size;
	if (write_inode->i_size == 0)
		new_block_sum = 0;
	else
		new_block_sum = (write_inode->i_size - 1) / BLOCK_SIZE + 1;

	/*需要注意的是，首次进入128需要额外分配*/
	/*这里相当于将物理块分配给逻辑块*/
	for (int logic_addr = old_block_sum; logic_addr < new_block_sum; logic_addr++)
	{
		if (logic_addr < ZERO_INDEX)//直接链接
		{
			write_inode->i_addr[logic_addr] = Func_AllocateBlock();//直接分配block
		}
		else if (logic_addr < ONE_INDEX)
		{
			int index0 = (logic_addr - ZERO_INDEX) / ONE_INDEX_NUM + 6;//6 or 7，零级索引
			int index1 = (logic_addr - ZERO_INDEX) % ONE_INDEX_NUM;//一级索引

			if (index1 == 0)//额外加一级索引块
			{
				write_inode->i_addr[index0] = Func_AllocateBlock();//1索引块
			}
			int block_num1 = write_inode->i_addr[index0];
			int block1[BLOCK_INT_NUM];
			/*先读入，修改，再写*/
			fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
			fd.read((char*)&block1, sizeof(block1));
			block1[index1] = Func_AllocateBlock();//数据块
			//写一级索引block位置
			fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
			fd.write((char*)&block1, sizeof(block1));
		}
		else if (logic_addr < TWO_INDEX)
		{
			int index0 = (logic_addr - ONE_INDEX) / TWO_INDEX_NUM + 8;//8 or 9，零级索引
			int index1 = ((logic_addr - ONE_INDEX) % TWO_INDEX_NUM) / ONE_INDEX_NUM;//一级索引 0-127
			int index2 = ((logic_addr - ONE_INDEX) % TWO_INDEX_NUM) % ONE_INDEX_NUM;//二级索引 0-127

			if (index1 == 0 && index2 == 0)//一级索引、二级索引
			{
				write_inode->i_addr[index0] = Func_AllocateBlock();//1索引块
				int block_num1 = write_inode->i_addr[index0];
				int block1[BLOCK_INT_NUM];
				/*先读入，修改，再写*/
				fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
				fd.read((char*)&block1, sizeof(block1));
				block1[index1] = Func_AllocateBlock();//2索引块
				//写一级索引block位置
				fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
				fd.write((char*)&block1, sizeof(block1));
				//写二级索引block位置
				int block_num2 = block1[index1];
				int block2[BLOCK_INT_NUM];
				/*先读入，修改，再写*/
				fd.seekg((BLOCK_START + block_num2) * BLOCK_SIZE, ios::beg);
				fd.read((char*)&block2, sizeof(block2));
				block2[index2] = Func_AllocateBlock();//数据块
				fd.seekg((BLOCK_START + block_num2) * BLOCK_SIZE, ios::beg);
				fd.write((char*)&block2, sizeof(block2));
			}
			else if (index2 == 0)//二级索引
			{
				int block_num1 = write_inode->i_addr[index0];
				int block1[BLOCK_INT_NUM];
				/*先读入，修改，再写*/
				fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
				fd.read((char*)&block1, sizeof(block1));
				block1[index1] = Func_AllocateBlock();//2索引块
				//写一级索引block位置
				fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
				fd.write((char*)&block1, sizeof(block1));
				//写二级索引block位置
				int block_num2 = block1[index1];
				int block2[BLOCK_INT_NUM];
				/*先读入，修改，再写*/
				fd.seekg((BLOCK_START + block_num2) * BLOCK_SIZE, ios::beg);
				fd.read((char*)&block2, sizeof(block2));
				block2[index2] = Func_AllocateBlock();//数据块
				fd.seekg((BLOCK_START + block_num2) * BLOCK_SIZE, ios::beg);
				fd.write((char*)&block2, sizeof(block2));
			}
			else
			{
				int block_num1 = write_inode->i_addr[index0];
				int block1[BLOCK_INT_NUM];
				/*先读入*/
				fd.seekg((BLOCK_START + block_num1) * BLOCK_SIZE, ios::beg);
				fd.read((char*)&block1, sizeof(block1));
				//写二级索引block位置
				int block_num2 = block1[index1];
				int block2[BLOCK_INT_NUM];
				/*先读入，修改，再写*/
				fd.seekg((BLOCK_START + block_num2) * BLOCK_SIZE, ios::beg);
				fd.read((char*)&block2, sizeof(block2));
				block2[index2] = Func_AllocateBlock();//数据块
				fd.seekg((BLOCK_START + block_num2) * BLOCK_SIZE, ios::beg);
				fd.write((char*)&block2, sizeof(block2));
			}
		}
		else//error
		{
			//cout << "error!!!" << endl;
			return -1;
		}
	}

	fd.close();
	return 1;
}


/*返回值：若存在返回对应数组下标，不存在则返回-1*/
int FileSystem::Func_CheckUser(string name)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);

	for (int i = 0; i < user_list.user_num; i++)
	{
		if (strcmp(user_list.user[i].name, name.c_str()) == 0)//该用户在文件中存在
		{
			fd.close();//关文件
			return i;
		}
	}
	fd.close();//关文件
	return -1;
}

/*返回值：若用户名与密码匹配则返回pos位置，不匹配则返回-1*/
int FileSystem::Func_UserToken(string name, string passwd)
{
	int pos = Func_CheckUser(name);
	if (pos == -1)
	{
		return -1;
	}

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);

	if (strcmp(user_list.user[pos].name, name.c_str()) == 0 && strcmp(user_list.user[pos].password, passwd.c_str()) == 0)
	{
		fd.close();//关文件
		return pos;
	}
	fd.close();//关文件
	return -1;
}


string FileSystem::Func_LastPath(string str)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	vector <string> path_list;//以vector形式分解的路径
	int pos;
	string sub_path = "";
	int start_inode;//起始位置的inode
	if (str[0] == '/')//绝对路径（从根目录开始）
	{
		//path_list.push_back("~");//先插入根目录
		start_inode = 0;//根目录inode
		for (pos = 1; pos <= str.size(); pos++)
		{
			if (pos == str.size())
			{
				if (sub_path == "")
					break;
				path_list.push_back(sub_path);
			}
			if (str[pos] == '/')//分界符
			{
				path_list.push_back(sub_path);
				sub_path = "";
			}
			else
			{
				sub_path += str[pos];
			}
		}
	}
	else//相对路径（从当前开始）
	{
		//path_list.push_back(now_dir.m_name);//先插入当前目录
		start_inode = now_dir.d_inode;//当前路径
		for (pos = 0; pos <= str.size(); pos++)
		{
			if (pos == str.size())
			{
				if (sub_path == "")
					break;
				path_list.push_back(sub_path);
			}
			if (str[pos] == '/')//分界符
			{
				path_list.push_back(sub_path);
				sub_path = "";
			}
			else
			{
				sub_path += str[pos];
			}
		}
	}
	fd.close();
	return path_list[path_list.size() - 1];
}

/*先测自己，再测同一用户组，再测其他
返回值：-1不可以，1可以*/
int FileSystem::Func_AuthCheck(int file_inode, string str)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + file_inode * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));

	char file_group[12];
	strcpy(file_group, Func_FromUseridGetGroup(inode.i_uid));

	int ret = -1;
	if (inode.i_uid == user_now.id)//是文件所有者
	{
		if (str == "read")
		{
			if ((inode.i_mode & Inode::INODEMODE::OWNER_r) == Inode::INODEMODE::OWNER_r)
				ret = 1;
		}
		else//write
		{
			if ((inode.i_mode & Inode::INODEMODE::OWNER_w) == Inode::INODEMODE::OWNER_w)
				ret = 1;
		}
	}
	else if(strcmp(user_now.usergroup, file_group)==0)//在同一用户组
	{
		if (str == "read")
		{
			if ((inode.i_mode & Inode::INODEMODE::GROUP_r) == Inode::INODEMODE::GROUP_r)
				ret = 1;
		}
		else//write
		{
			if ((inode.i_mode & Inode::INODEMODE::GROUP_w) == Inode::INODEMODE::GROUP_w)
				ret = 1;
		}
	}
	else//其他人
	{
		if (str == "read")
		{
			if ((inode.i_mode & Inode::INODEMODE::OTHER_r) == Inode::INODEMODE::OTHER_r)
				ret = 1;
		}
		else//write
		{
			if ((inode.i_mode & Inode::INODEMODE::OTHER_w) == Inode::INODEMODE::OTHER_w)
				ret = 1;
		}
	}

	fd.close();//关文件
	return ret;
}

/*根据uid获取groupname*/
const char* FileSystem::Func_FromUseridGetGroup(int user_id)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);
	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);
	for (int i = 0; i < user_list.user_num; i++)
	{
		if (user_list.user[i].id == user_id)
		{
			fd.close();//关文件
			return user_list.user[i].usergroup;
		}
	}

	fd.close();//关文件
	return "error";
}
