#include "EasyTcpClient.hpp"

bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char szRecv[128] = {};
		scanf("%s", szRecv);
		if (0 == strcmp(szRecv, "login"))
		{
			/*    Login login;
			 strcpy(login.userName, "yhp");
			 strcpy(login.password, "yhpmm");
			 client->SendData(&login);*/
		}
		else if (0 == strcmp(szRecv, "loginout"))
		{
			/*    Loginout loginout;
			 strcpy(loginout.userName, "yhp");
			 client->SendData(&loginout);*/
		}
		else if (0 == strcmp(szRecv, "exit"))
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

//�����߳�����
const int tCount = 4;	
//�ͻ�������
const int cCount = 400;
//�ͻ�������
EasyTcpClient* client[cCount];

void sendThread(int id)
{
	//4���߳� ID 1-4
	int c = cCount / tCount;
	int begin = (id - 1)*c;
	int end = id * c;
	for (int i = begin; i < end; i++)
	{
		client[i] = new EasyTcpClient;
	}

	for (int i = begin; i < end; i++)
	{
		client[i]->Connect("127.0.0.1", 4567);
		printf("Connect=%d\n", i);
	}

	Login login;
	strcpy(login.userName, "yhp");
	strcpy(login.password, "yhpmm");

	while (g_bRun)
	{
		for (int i = begin; i < end; i++)
		{
			client[i]->SendData(&login);
		}
	}

	for (int i = begin; i < end; i++)
	{
		client[i]->Close();
		delete client[i];
	}
}

int main()
{

	//����UI�߳�
	std::thread td(cmdThread);
	td.detach();

	for (int i = 0; i < tCount; i++)
	{
		std::thread t(sendThread, i + 1);
		t.detach();
	}

	//while (g_bRun)
	//{
	//	Sleep(100);
	//}
	while (true)
	{
	}
	printf("���˳���");
	return 0;
}

