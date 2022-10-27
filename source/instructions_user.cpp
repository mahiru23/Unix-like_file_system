#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"


/*md5考虑使用*/
const char* UserInfo::Func_Encryption(const char password1[12])
{
	return password1;
}



/*多用户相关处理*/
//在用户block中增加用户内容
int FileSystem::Func_AddUser(vector<string> str)
{
	//username、password两个参数
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}
	if (str[1].size() > 11)
	{
		cout << "用户名过长，限制在11个字节内" << endl;
		return - 1;
	}
	if (Func_CheckUser(str[1]) != -1)
	{
		cout << "用户名已存在，创建失败" << endl;
		return -1;
	}
	/*if (strcmp(user_now.name, "root") != 0)
	{
		cout << "只有root用户才能创建用户，创建失败" << endl;
		return -1;
	}*/


	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);

	if (user_list.user_num == MAX_USER_NUM)
	{
		cout << "当前系统用户数量已达上限" << endl;
		fd.close();//关文件
		return -1;
	}


	UserInfo new_user;
	new_user.id = user_list.user_now;
	user_list.user_now++;
	strcpy(new_user.name, str[1].c_str());

	const char* crypassword = new_user.Func_Encryption(str[2].c_str());
	strcpy(new_user.password, crypassword);

	new_user.usergroup[0] = '\0';

	memcpy(&user_list.user[user_list.user_num], &new_user, sizeof(UserInfo));
	user_list.user_num++;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&user_list, BLOCK_SIZE);


	fd.close();//关文件
	cout << "创建新用户成功" << endl;
	return 1;
}


//在用户block中删除用户
int FileSystem::Func_DelUser(vector<string> str)
{
	//username参数
	if (Func_CheckParaNum(str, 1) == -1)
	{
		return -1;
	}

	/*删除*/
	int pos = Func_CheckUser(str[1]);
	if (pos == -1)
	{
		cout << "用户不存在，删除失败" << endl;
		return -1;
	}
	if (strcmp(user_now.name, str[1].c_str()) == 0)
	{
		cout << "用户处在登录状态，删除失败" << endl;
		return -1;
	}
	if (strcmp(user_now.name, "root") != 0)
	{
		cout << "只有root用户才能删除其他用户，删除失败" << endl;
		return -1;
	}


	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);
	user_list.user_num--;//先自减，用自减后的数
	memcpy(&user_list.user[pos], &user_list.user[user_list.user_num], sizeof(UserInfo));


	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&user_list, BLOCK_SIZE);
	fd.close();//关文件
	cout << "删除用户成功" << endl;
	return 1;
}


//在用户block中添加/修改用户组
int FileSystem::Func_AddUserToGroup(vector<string> str)
{
	//username参数
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}

	/*删除*/
	int pos = Func_CheckUser(str[1]);
	if (pos == -1)
	{
		cout << "用户不存在，添加用户组失败" << endl;
		return -1;
	}
	if (str[2].size() > 11)
	{
		cout << "用户组名过长，限制在11个字节内" << endl;
		return -1;
	}


	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);

	strcpy(user_list.user[pos].usergroup, str[2].c_str());
	if (strcmp(user_now.name, str[1].c_str()) == 0)
	{
		strcpy(user_now.usergroup, str[2].c_str());
	}

	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&user_list, BLOCK_SIZE);
	fd.close();//关文件
	cout << "添加成功" << endl;
	return 1;
}


//在用户block中删除用户组中的用户
int FileSystem::Func_DelUserFromGroup(vector<string> str)
{
	//username参数
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}

	/*删除*/
	int pos = Func_CheckUser(str[1]);
	if (pos == -1)
	{
		cout << "用户不存在，删除失败" << endl;
		return -1;
	}

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);


	if (strcmp(user_list.user[pos].usergroup, str[2].c_str()) == 0)
	{
		user_list.user[pos].usergroup[0] = '\0';
		if (strcmp(user_now.name, str[1].c_str()) == 0)
		{
			user_now.usergroup[0] = '\0';
		}
	}
	else
	{
		fd.close();//关文件
		cout << "删除失败，用户不属于该用户组" << endl;
		return -1;
	}

	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.write((char*)&user_list, BLOCK_SIZE);
	fd.close();//关文件
	cout << "删除成功" << endl;
	return 1;
}


/*打印当前用户用户名*/
int FileSystem::Func_Whoami(vector<string> str)
{
	if (Func_CheckParaNum(str, 0) == -1)
	{
		return -1;
	}

	cout << user_now.name << endl;
	return 1;
}


/*打印所有用户*/
int FileSystem::Func_ShowAllUser(vector<string> str)
{
	if (Func_CheckParaNum(str, 0) == -1)
	{
		return -1;
	}


	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);

	cout << setw(16) << left << "user_id";
	cout << setw(16) << left << "user_name";
	cout << setw(16) << left << "user_group";
	cout << endl;

	for (int i = 0; i < user_list.user_num; i++)
	{
		user_list.user[i].id;
		//打印id
		cout << setw(16) << left << user_list.user[i].id;

		//打印名字
		cout << setw(16) << left << user_list.user[i].name;

		//打印group
		if (user_list.user[i].usergroup[0] != '\0')
			cout << setw(16) << left << user_list.user[i].usergroup;
		else
			cout << setw(16) << left << "null";
		cout << right;

		cout << endl;
	}
	
	fd.close();//关文件
	return 1;
}

/*需要同时输入账号和密码*/
int FileSystem::Func_Su(vector<string> str)
{
	if (Func_CheckParaNum(str, 2) == -1)
	{
		return -1;
	}

	int pos = Func_UserToken(str[1], str[2]);
	if (pos == -1)
	{
		cout << "账号或密码有误，请重新输入" << endl;
		return -1;
	}

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);

	memcpy(&user_now, &user_list.user[pos], sizeof(UserInfo));


	fd.close();//关文件
	return 1;

}

int FileSystem::Func_UserExit(vector<string> str)
{
	if (Func_CheckParaNum(str, 0) == -1)
	{
		return -1;
	}

	cout << "已退出文件系统" << endl;
	return EXIT_SHELL;
}

int FileSystem::Func_UpdateUserPassword(vector<string> str)
{
	if (Func_CheckParaNum(str, 1) == -1)
	{
		return -1;
	}
	/*删除*/
	int pos = Func_CheckUser(str[1]);
	if (pos == -1)
	{
		cout << "用户不存在，修改失败" << endl;
		return -1;
	}

	string old_password;
	string new_password;
	cout << "旧密码: ";
	cin >> old_password;
	cout << "新密码: ";
	cin >> new_password;

	fstream fd;//c++方式的文件流，继承自iostream
	fd.open(MYDISK_NAME, ios::out | ios::in | ios::binary);

	Users user_list;
	fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
	fd.read((char*)&user_list, BLOCK_SIZE);


	if (strcmp(user_list.user[pos].password, user_list.user[pos].Func_Encryption(old_password.c_str())) == 0)
	{
		strcpy(user_list.user[pos].password, user_list.user[pos].Func_Encryption(new_password.c_str()));
		strcpy(user_now.password, user_list.user[pos].Func_Encryption(new_password.c_str()));
		fd.seekg((BLOCK_START + USER_INFO_BLOCK) * BLOCK_SIZE, ios::beg);
		fd.write((char*)&user_list, BLOCK_SIZE);
	}
	else
	{
		cout << "旧密码有误" << endl;
	}

	fd.close();//关文件
	return 1;
}