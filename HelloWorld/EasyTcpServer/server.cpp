#include "EasyTcpServer.hpp"

bool g_bRun = true;

void cmdThread()
{
	while (true)
	{
		char cmdBuf[128] = {};
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			printf("退出cmdThread线程\n");
			break;
		}
		else
		{
			printf("不支持的命令\n");
		}
	}
}

int main()
{
	EasyTcpServer server;
	server.InitSocket();
	server.Bind(NULL, 4567);
	server.Listen(4000);
	server.Start();

	//启动UI线程
	std::thread td(cmdThread);
	td.detach();

	while (g_bRun)
	{
		server.onRun();
	}
	server.Close();
	printf("已退出。");
	getchar();
	return 0;
}

