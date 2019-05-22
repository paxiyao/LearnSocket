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
			printf("�˳�cmdThread�߳�\n");
			break;
		}
		else
		{
			printf("��֧�ֵ�����\n");
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

	//����UI�߳�
	std::thread td(cmdThread);
	td.detach();

	while (g_bRun)
	{
		server.onRun();
	}
	server.Close();
	printf("���˳���");
	getchar();
	return 0;
}

