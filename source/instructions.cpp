#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"


/*指令集1，存放目录相关指令*/

//help指令，输出help对应的提示信息
int FileSystem::Func_Help(vector<string> str)
{
	if (str.size() == 1)//空白help指令，打印指令集简要信息
	{
		//基本操作
		cout << "支持指令         指令格式                       指令功能" << endl;
		cout << "help                                            提示信息" << endl;
		cout << "cd               <文件路径>                     切换当前目录至指定路径" << endl;
		cout << "ls                                              展示当前目录下内容" << endl;
		cout << "mkdir            <文件路径>                     在指定路径下创建目录" << endl;
		cout << "rm               <文件路径>                     删除指定路径下的目录或文件" << endl;
		cout << "chmod            <权限>     <文件路径>          修改目录或文件的权限" << endl;

		//文件操作
		cout << "create           <文件路径>                     在指定路径下创建文件" << endl;
		cout << "open             <文件路径>                     打开指定文件" << endl;
		cout << "close            <文件路径>                     关闭指定文件" << endl;
		cout << "cat              <文件路径>                     打印指定文件所有内容" << endl;
		cout << "read             <文件路径> <读长度>            读文件" << endl;
		cout << "write            <文件路径> <写内容>            写文件" << endl;
		cout << "lseek            <文件路径> <新的文件指针位置>  移动文件指针" << endl;
		cout << "writein          <文件路径> <外部文件路径>      从外部导入文件进入文件系统" << endl;
		cout << "writeout         <文件路径> <外部文件路径>      从文件系统向外部导出文件" << endl;
		cout << "opened                                          查看打开文件信息" << endl;

		//多用户相关操作
		cout << "adduser          <用户名>   <用户密码>          增加用户" << endl;
		cout << "deluser          <用户名>                       删除用户" << endl;
		cout << "addusertogroup   <用户名>   <用户组名>          增加用户进用户组" << endl;
		cout << "deluserfromgroup <用户名>   <用户组名>          从用户组中删除用户" << endl;
		cout << "whoami                                          打印当前用户" << endl;
		cout << "showalluser                                     打印所有用户信息" << endl;
		cout << "updatepassword   <用户名>                       修改用户密码" << endl;
		cout << "su               <用户名>                       切换当前登录用户" << endl;
		cout << "exit                                            退出文件系统" << endl;

	}
	else
	{

	}
	return 1;
}


int FileSystem::Func_Mkdir(vector<string> str)
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

	string new_dir_name = Func_LastPath(str[1]);

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
	inode1.i_mode = 0x3FF;					//目录，具体权限之后考虑
	inode1.i_nlink = 0;						//文件在不同目录树中的索引数量
	inode1.i_uid = user_now.id;						//文件所有者用户id
	//inode1.i_gid = user_now.groupid;						//文件所有者用户组id
	inode1.i_size = DIRECTORY_SIZE;			//文件大小，目录文件为512
	inode1.i_addr[0] = Func_AllocateBlock();	//分配block块

	inode1.i_no = my_superblock.s_inode[--my_superblock.s_ninode];//栈顶inode序号，top操作
	//my_superblock.s_ninode--;//等价出栈，pop操作
	inode1.i_atime = time(NULL);			//最后访问时间
	inode1.i_mtime = time(NULL);			//最后修改时间

	//inode放入inode区对应位置中
	fd.seekg(INODE_START * BLOCK_SIZE + inode1.i_no * sizeof(Inode), ios::beg);
	fd.write((char*)&inode1, sizeof(Inode));

	//初始化Directory目录结构，将根目录写入block
	//需要注意的是，在所有目录中都有"."和".."代表本目录和上一级目录，但根目录特殊，均代表自己
	Directory new_directory;
	strcpy(new_directory.m_name, new_dir_name.c_str());//目录名写入
	new_directory.d_inode = inode1.i_no;//inode序号写入
	//目录.为自己，指向自己inode
	strcpy(new_directory.child_filedir[0].m_name, ".");
	new_directory.child_filedir[0].n_ino = inode1.i_no;
	//..为父目录，指向当前目录
	strcpy(new_directory.child_filedir[1].m_name, "..");
	new_directory.child_filedir[1].n_ino = father_dir.d_inode;
	//初始化其他位置为0
	for (int i = 2; i < MAX_SUBDIR; i++) {
		new_directory.child_filedir[i].m_name[0] = '\0';
		new_directory.child_filedir[i].n_ino = -1;//-1代表没有对应的inode（在查找时记得做判断）
	}
	fd.seekg((BLOCK_START + inode1.i_addr[0]) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&new_directory, sizeof(new_directory));

	//更新father目录
	strcpy(father_dir.child_filedir[father_dir_insert].m_name, new_dir_name.c_str());
	father_dir.child_filedir[father_dir_insert].n_ino = inode1.i_no;

	if (now_dir.d_inode == father_inode_num)//记得更新当前所在目录(如果相同)
	{
		memcpy(&now_dir, &father_dir, sizeof(Directory));
	}

	fd.seekg((BLOCK_START + father_inode.i_addr[0]) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&father_dir, sizeof(father_dir));

	fd.close();//关文件

	Func_RenewDisk();//不要写在上面打开的文件里面，否则造成重复关闭

	cout << "创建新文件夹成功" << endl;
	return 1;
}


int FileSystem::Func_Rm_plus(int inode_num, int father_inode_num)
{
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));
	if ((inode.i_mode & Inode::INODEMODE::DIRECTORY) == Inode::INODEMODE::DIRECTORY)//目录，递归删除所有内容(记得最后删除自己)
	{
		Directory dir;
		fd.seekg((BLOCK_START + inode.i_addr[0]) * BLOCK_SIZE, ios::beg);
		fd.read((char*)&dir, sizeof(dir));
		//删除子目录和文件
		for (int i = MAX_SUBDIR - 1; i >= 2; i--)//反向删除！！！因为过程中有位置的向前替换
		{
			if (dir.child_filedir[i].n_ino != -1)
			{
				Func_Rm_plus(dir.child_filedir[i].n_ino, inode_num);
			}
		}
		//删除本目录

	}
	else//文件，删除单个文件，同时删除inode（维护dir！！！）
	{
		;//在下面进行统一删除
	}

	//根据文件的size计算block_sum
	int block_sum;//该文件占用的数据区block数
	if (inode.i_size == 0)
		block_sum = 0;
	else
		block_sum = (inode.i_size - 1) / BLOCK_SIZE + 1;

	//一个一个的删除block
	for (int i = 0; i < block_sum; i++)
	{
		int del_block_addr = Func_AddrMapping(inode_num, i);
		Func_ReleaseBlock(del_block_addr);//delete
	}

	//删除父亲dir中的对应记录
	Inode father_inode;
	Directory father_dir;
	fd.seekg(INODE_START * BLOCK_SIZE + father_inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&father_inode, sizeof(Inode));
	fd.seekg((BLOCK_START + father_inode.i_addr[0]) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&father_dir, sizeof(father_dir));

	int pos_now;//要删除的inode在父目录位置
	int pos_end = MAX_SUBDIR - 1;//父目录最后一个inode位置（将其移到前面来）
	for (int i = 0; i < MAX_SUBDIR; i++)
	{
		if (father_dir.child_filedir[i].n_ino == inode_num)
		{
			pos_now = i;
		}
		if (father_dir.child_filedir[i].n_ino == -1)
		{
			pos_end = i - 1;
			break;
		}
	}
	father_dir.child_filedir[pos_now].n_ino = father_dir.child_filedir[pos_end].n_ino;
	strcpy(father_dir.child_filedir[pos_now].m_name, father_dir.child_filedir[pos_end].m_name);
	father_dir.child_filedir[pos_end].n_ino = -1;
	strcpy(father_dir.child_filedir[pos_end].m_name, "");

	if (now_dir.d_inode == father_inode_num)//记得更新当前所在目录
	{
		memcpy(&now_dir, &father_dir, sizeof(Directory));
	}
	//删除inode
	my_superblock.s_inode[my_superblock.s_ninode] = inode_num;
	my_superblock.s_ninode++;

	fd.close();
	Func_RenewDisk();//保存全局
	return 1;
}


int FileSystem::Func_Rm(vector<string> str)
{
	if (Func_CheckParaNum(str, 1) == -1)
	{
		return -1;
	}
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	/*首先判断是否是当前目录*/
	/*根目录不能删除，root可以删除除了根目录以外的其他任何文件，而普通用户只能删除自己是所有者的文件*/
	/*注意在删除文件时记得同时删除inode*/
	/*删除目录时使用递归方法删除其中所有文件*/
	int rm_inode_num = Func_AnalysePath(str[1]);
	if (rm_inode_num == -1)
	{
		cout << "路径不存在，重新输入！" << endl;
		fd.close();
		return 1;
	}
	if (rm_inode_num == now_dir.d_inode)
	{
		cout << "不能删除当前目录" << endl;
		fd.close();
		return 1;
	}
	if (rm_inode_num == 0)
	{
		cout << "不能删除根目录" << endl;
		fd.close();
		return 1;
	}
	if (rm_inode_num == -1)
	{
		cout << "要删除的文件或目录不存在" << endl;
		fd.close();
		return 1;
	}

	int open_pos = -1;
	for (int i = 0; i < inode_open_num; i++)
	{
		if (rm_inode_num == my_inode_map[i].i_no)
		{
			open_pos = i;
			break;
		}
	}
	if (open_pos != -1)
	{
		cout << "该文件已打开，请先关闭" << endl;
		fd.close();
		return -1;
	}

	int father_inode_num = Func_GetFatherInode(str[1]);
	if (father_inode_num == -1)
	{
		cout << "路径不存在，重新输入！" << endl;
		fd.close();
		return 1;
	}
	Func_Rm_plus(rm_inode_num, father_inode_num);


	fd.close();
	Func_RenewDisk();//保存全局
	return 1;
}
int FileSystem::Func_Ls(vector<string> str)
{
	//Func_CheckParaNum(str, 0);
	/*打印内容包括权限（10位），名，大小，最后修改时间
	.和..暂时不考虑打印*/
	cout << "privilege\tname\t\tsize\tupdatetime" << endl;
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);
	for (int i = 2; i < MAX_SUBDIR; i++)
	{
		if (now_dir.child_filedir[i].m_name[0] != '\0')
		{

			Inode inode;
			fd.seekg(INODE_START * BLOCK_SIZE + now_dir.child_filedir[i].n_ino * sizeof(Inode), ios::beg);
			fd.read((char*)&inode, sizeof(Inode));


			//打印权限
			for (int j = 0; j < 10; j++)
			{
				if ((inode.i_mode & (512 >> j)) != 0)//使用按位与操作
					cout << PRIVILEGE[j];
				else
					cout << "-";
			}

			//打印名字
			cout << "\t";
			cout << setw(16) << left << now_dir.child_filedir[i].m_name;
			cout << right;


			//打印size
			if ((inode.i_mode & 512) != 0)//使用按位与操作
				cout << "-";
			else
				cout << inode.i_size;

			//打印修改time
			cout << "\t" << ctime(&inode.i_mtime);

			//cout << endl;
		}
		else
		{
			break;
		}
	}

	fd.close();
	Func_RenewDisk();//不要写在上面打开的文件里面，否则造成重复关闭
	return 1;
}
int FileSystem::Func_Cd(vector<string> str)
{
	if (Func_CheckParaNum(str, 1) == -1)
	{
		return -1;
	}
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	//now_dir.;
	int new_inode = Func_AnalysePath(str[1]);
	if (new_inode == -1)
	{
		cout << "路径不存在，重新输入！" << endl;
		fd.close();
		return 1;
	}
	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + new_inode * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));
	if ((inode.i_mode & (Inode::INODEMODE::DIRECTORY)) != Inode::INODEMODE::DIRECTORY)//不是目录，无法进入
	{
		cout << "不是目录，无法进入" << endl;
		fd.close();
		Func_RenewDisk();
		return 1;
	}

	int newdir_block_num = Func_AddrMapping(new_inode, 0);//dir只有[0]
	fd.seekg((BLOCK_START + newdir_block_num) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&now_dir, sizeof(now_dir));


	fd.close();
	Func_RenewDisk();
	return 1;
}

int FileSystem::Func_Chmod(vector<string> str)
{
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}
	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	/*修改文件权限，只支持修改自己作为所有者的文件
	root特例，可以修改所有文件的权限
	输入参数（3位八进制数（777））*/
	int new_mode = 0;
	if (str[1].size() != 3)
	{
		cout << "文件权限参数格式有误，请重新输入" << endl;
		return -1;
	}

	for (int i = 2; i >=0 ;i--)
	{
		if (str[1][i] < '0' || str[1][i] > '7')
		{
			cout << "文件权限参数格式有误，请重新输入" << endl;
			return -1;
		}
		new_mode += (str[1][i] - '0') * (1 << (3 * (2 - i)));
	}
	//int new_mode;

	/*先找路径*/
	int inode_num = Func_AnalysePath(str[2]);
	if (inode_num == -1)
	{
		cout << "路径不存在，重新输入！" << endl;
		fd.close();
		return -1;
	}

	Inode inode;
	fd.seekg(INODE_START * BLOCK_SIZE + inode_num * sizeof(Inode), ios::beg);
	fd.read((char*)&inode, sizeof(Inode));

	new_mode += (inode.i_mode & (Inode::INODEMODE::DIRECTORY));//dir属性保留
	inode.i_mode = new_mode;

	fd.seekg(INODE_START * BLOCK_SIZE + inode_num * sizeof(Inode), ios::beg);
	fd.write((char*)&inode, sizeof(Inode));

	fd.close();
	return 1;
}







