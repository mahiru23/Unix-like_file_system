#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"

/*shell相关，存放包括初始化在内的shell相关操作*/

//分解一个string，将参数串转换为参数列表
//stringstream字符流，用于字符串的拼接及裂变
vector<string> split(string str)
{
	string buf;
	stringstream ss(str);
	vector<string> v;
	// 字符流ss 
	while (ss >> buf) {
		//转小写
		transform(buf.begin(), buf.end(), buf.begin(), ::tolower);
		v.push_back(buf);
	}

	return v;
}


int FileSystem::Func_LoginLoop()//循环登录
{
	Func_InitInstructions();//初始化指令集
	//user_num = 1;
	//user[0].Func_FillUser("root", "root", 0);

	char whe_init;
	cout << "是否重新构建磁盘？（y：重新构建，n：不重新构建）：";
	cin >> whe_init;
	if (whe_init == 'y')
		Func_CreateNewFileSystem();//重新加载
	else
		Func_RenewMemory();//不重新加载

	while (1)
	{
		string user_name;
		string password;
		cout << "登录" << endl;
		cout << "用户名: ";
		cin >> user_name;
		cout << "密码: ";
		cin >> password;

		vector <string> str_login(3);
		str_login[0] = "su";
		str_login[1] = user_name;
		str_login[2] = password;
		int ret = Func_Su(str_login);
		if (ret == 1)//登录成功
		{
			break;
		}
	}

	return 1;
}

int FileSystem::Func_ShellLoop()
{
	inode_open_num = 0;
	cout << "欢迎登录，可以通过输入help查看本文件系统支持的指令" << endl;


	char c; 
	while ((c = getchar()) != '\n');//清空输入缓冲区

	while (1)
	{
		string para_string;//参数列表
		cout << "[" << user_now.name <<" "<< now_dir.m_name<< "]" << "#";

		getline(cin, para_string);
		vector<string> para_list = split(para_string);//获取参数列表

		if (para_list.empty())//只有回车，重新开始循环
			continue;
		/*有效，开始进行参数处理*/
		if (inst.find(para_list[0]) == inst.end())//在指令集中查找指令是否在里面
		{
			cout << "参数" << para_list[0] << "不合法，请重新考虑" << endl;
			continue;
		}

		//参数合法
		int k = (this->*inst[para_list[0]])(para_list);//进入具体函数进行处理
		if (k == EXIT_SHELL)
		{
			break;
		}
	}
	return 1;
}

//创建新的文件系统（磁盘文件标准格式化）
int FileSystem::Func_CreateNewFileSystem()
{
	/*首先创建对应大小的磁盘文件，在进入函数之前已经确定没有文件存在*/
	/*磁盘文件规格：（.h文件中宏定义规定）
	superblock：512*2
	inode:512*10
	数据存储区：512*100
	*/

	//在当前目新建文件作为文件卷
	fstream fd(MYDISK_NAME, ios::out);//c++方式的文件流，继承自iostream
	fd.close();
	fd.open(MYDISK_NAME, ios::out  | ios::in | ios::binary);//不存在则创建，使用ios::_Nocreate令存在情况下open失败
	//如果没有打开文件则输出提示信息并throw错误
	if (!fd.is_open()) //错误检测
	{
		cout << "磁盘文件初始化失败" << endl;
		exit(-1);
	}

	//对SuperBlock进行初始化
	SuperBlock superblock;
	superblock.disk_block_size = SUPERBLOCK_BLOCK_NUM + INODE_BLOCK_NUM + BLOCK_NUM;

	//初始化superblock的inode相关项
	superblock.s_isize = INODE_BLOCK_NUM;
	superblock.s_ninode = INODE_NUM - 1;//起始创建根目录和用户文件
	for (int i = 0; i < INODE_NUM; i++)
	{
		superblock.s_inode[i] = (INODE_NUM - i - 1);//栈式管理，反向--定义
	}

	//初始化superblock的block相关项，使用成组链接
	superblock.s_fsize = 2;
	superblock.s_nfree = BLOCK_FREE_NUM - 2;
	//数据块block成组链接,每组[0]位置的指向下一个块
	/*数据区block块分为两类，链接块和数据块，链接块每隔100有一个*/
	int now_pos = 0;//当前组的位置
	int* my_list = superblock.s_free;//循环过程中的组
	int last_write_pos = BLOCK_START;//写入位置为上一组起始处
	BlockGroup BG;
	int whe_super = 0;//只起一次作用
	for (int i = 0; ; i++)
	{
		if (i == BLOCK_NUM)//此处已经到达末尾且没有其他要处理的内容
		{
			BG.s_nfree = now_pos;
			fd.seekg(BLOCK_SIZE * last_write_pos, ios::beg);
			fd.write((char*)&BG, sizeof(BG));
			break;
		}
		if (now_pos == BLOCK_FREE_NUM)//当前组满了，写入
		{
			//写入
			if (whe_super == 0)//superblock，后续写入，更换list
			{
				whe_super = 1;
				my_list = BG.s_free;
			}
			else//非superblock，直接写入，不更换list（但是记得清空）
			{
				fd.seekg(BLOCK_SIZE * last_write_pos, ios::beg);
				last_write_pos += BLOCK_FREE_NUM;//??????
				BG.s_nfree = BLOCK_FREE_NUM;
				fd.write((char*)&BG, sizeof(BG));//转换成char*直接写入文件，需要读取时直接从文件中同样方式类型转换读到内存
				memset(my_list, 0, sizeof(my_list));
			}
			now_pos = 0;
			i--;//本次未处理
			continue;
		}
		my_list[now_pos] = i;
		now_pos++;
	}

	//初始化superblock的其他位置
	superblock.s_fmod = ~SUPERBLOCK_WRITEBACK;
	superblock.s_ronly = SUPERBLOCK_READWRITE;
	superblock.s_time = time(NULL);//time.h中定义time方法


	//对inode进行相关操作（暂时不支持成组链接，如果需要成组链接再修改）
	Inode inode1;//根目录
	//Inode inode2;//用户文件


	//根目录inode初始化
	inode1.i_mode = 0x3FF;					//目录，具体权限之后考虑
	inode1.i_nlink = 0;						//文件在不同目录树中的索引数量
	inode1.i_uid = 0;						//文件所有者用户id
	//inode1.i_gid = 0;						//文件所有者用户组id
	inode1.i_size = DIRECTORY_SIZE;			//文件大小，目录文件为512
	inode1.i_addr[0] = superblock.s_free[BLOCK_FREE_NUM-1];//从倒数第一个数据块[99]开始
	inode1.i_no = 0;						//文件的inode序号（便于内存中inode写回磁盘查找位置）
	inode1.i_atime = time(NULL);			//最后访问时间
	inode1.i_mtime = time(NULL);			//最后修改时间
	//inode放入inode区对应位置中
	fd.seekg(INODE_START * BLOCK_SIZE + inode1.i_no * sizeof(Inode), ios::beg);
	fd.write((char*)&inode1, sizeof(Inode));

	//初始化Directory目录结构，将根目录写入block
	//需要注意的是，在所有目录中都有"."和".."代表本目录和上一级目录，但根目录特殊，均代表自己
	Directory root_directory;
	strcpy(root_directory.m_name, "~");//根目录用~来代替
	root_directory.d_inode = 0;
	//根目录.和..均为自己，均指向0号inode
	strcpy(root_directory.child_filedir[0].m_name, ".");
	root_directory.child_filedir[0].n_ino = 0;
	strcpy(root_directory.child_filedir[1].m_name, "..");
	root_directory.child_filedir[1].n_ino = 0;
	//strcpy(root_directory.child_filedir[2].m_name, "user_info");//用户文件
	//root_directory.child_filedir[2].n_ino = 1;
	//初始化其他位置为0
	for (int i = 2; i < MAX_SUBDIR; i++) {
		root_directory.child_filedir[i].m_name[0] = '\0';
		root_directory.child_filedir[i].n_ino = -1;//-1代表没有对应的inode（在查找时记得做判断）
	}
	//directory放入block数据区中
	fd.seekg((BLOCK_START + inode1.i_addr[0]) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&root_directory, sizeof(root_directory));

	//用户文件inode初始化
	/*inode2.i_mode = 0x1FF;					//目录，具体权限之后考虑
	inode2.i_nlink = 0;						//文件在不同目录树中的索引数量
	inode2.i_uid = 0;						//文件所有者用户id
	inode2.i_gid = 0;						//文件所有者用户组id
	const char userfile[40] = "uid\tusername\tpassword\r\n0\troot\troot123\r\n";//初始root用户
	inode2.i_size = sizeof(userfile);		//文件大小
	inode2.i_addr[0] = superblock.s_free[BLOCK_FREE_NUM-2];//从倒数第二个数据块[98]开始
	inode2.i_no = 1;						//文件的inode序号（便于内存中inode写回磁盘查找位置）
	inode2.i_atime = time(NULL);			//最后访问时间
	inode2.i_mtime = time(NULL);			//最后修改时间
	//inode写入inode区对应位置中
	fd.seekg(INODE_START * BLOCK_SIZE + inode2.i_no * sizeof(Inode), ios::beg);
	fd.write((char*)&inode2, sizeof(Inode));
	//将文件内容实际写入block
	fd.seekg((BLOCK_START + inode2.i_addr[0]) * BLOCK_SIZE, ios::beg);
	fd.write((char*)userfile, sizeof(userfile));*/

	//根目录和root用户初始化完成，准备进行其他要求目录和文件的创建
	//此时已经实现了相关的对应函数，直接调用相关api即可
	

	//superblock写入文件中
	fd.seekg(SUPERBLOCK_START * BLOCK_SIZE, ios::beg);
	fd.write((char*)&superblock, sizeof(superblock));//转换成char*直接写入文件，需要读取时直接从文件中同样方式类型转换读到内存
	//superblock更新内存
	fd.seekg(SUPERBLOCK_START * BLOCK_SIZE, ios::beg);
	fd.read((char*)&my_superblock, sizeof(SuperBlock));//转换成char*直接写入文件，需要读取时直接从文件中同样方式类型转换读到内存

	//进入根目录
	//now_path = "~";
	fd.seekg((BLOCK_START + inode1.i_addr[0])* BLOCK_SIZE, ios::beg);
	fd.read((char*)&now_dir, sizeof(now_dir));
	//path_inode = 0;

	//创建root用户
	vector <string> root_init;
	root_init.push_back("adduser");
	root_init.push_back("root");
	root_init.push_back("root123");
	Func_AddUser(root_init);

	//创建目录
	vector <string> init_dir;
	init_dir.push_back("mkdir");
	init_dir.push_back("/bin");
	Func_Mkdir(init_dir);
	init_dir[1] = "/etc";
	Func_Mkdir(init_dir);
	init_dir[1] = "/home";
	Func_Mkdir(init_dir);
	init_dir[1] = "/dev";
	Func_Mkdir(init_dir);
	init_dir[1] = "/home/texts";
	Func_Mkdir(init_dir);
	init_dir[1] = "/home/reports";
	Func_Mkdir(init_dir);
	init_dir[1] = "/home/photos";
	Func_Mkdir(init_dir);



	fd.close();//关文件
	return 0;
}



