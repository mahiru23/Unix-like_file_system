#include "filesystem.h"

int main()
{
	FileSystem::getInstance();//单例模式初始化

	/*登录循环*/
	FileSystem::getInstance().Func_LoginLoop();//循环登录

	/*进入shell，开始循环操作*/
	FileSystem::getInstance().Func_ShellLoop();

	return 0;
}



