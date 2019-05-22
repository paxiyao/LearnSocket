#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  //��������winsock�ĺ�����
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<windows.h>
#include<WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include<unistd.h>  //uni std
#include<arpa/inet.h>
#include<string.h>

#define SOCKET int
#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)
#endif

#include<stdio.h>
#include<thread>
#include "MessageHeader.hpp"

#ifndef RECV_BUFFER_SIZE
#define RECV_BUFFER_SIZE 10240    // ��������С��Ԫ�Ĵ�С
#endif // !RECV_BUFFER_SIZE

class EasyTcpClient
{
private:
	SOCKET _sock;
	char _szRecv[RECV_BUFFER_SIZE];          //���ջ�����
	char _szMsgBuf[RECV_BUFFER_SIZE * 10];    //�ڶ������� ��Ϣ������
	int _lastPos;                            //��Ϣ������β����λ��
public:
	EasyTcpClient()
	{
		_sock = INVALID_SOCKET;
		memset(_szRecv, 0, RECV_BUFFER_SIZE);
		memset(_szMsgBuf, 0, RECV_BUFFER_SIZE * 10);
		_lastPos = 0;
	}

	// ����������
	virtual ~EasyTcpClient()
	{
		Close();
	}

	// ��ʼ��socket
	int InitSocket()
	{
#ifdef _WIN32
		// ����window socket 2.x����
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			printf("<Socket=%d>�رվ�����...\n", _sock);
			Close();
		}
		// 1 ����һ�� socket
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("����,����Socketʧ��...\n");
		}
		else
		{
			//printf("<socket=%d>����socket�ɹ�...\n", _sock);
		}
		return _sock;
	}

	// ���ӷ�����
	int Connect(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}
		// 2 ���ӷ����� connect
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);
#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			printf("����,����Socketʧ��...\n");
		}
		else
		{
			//printf("<socket=%d>����socket�ɹ�...\n", _sock);
		}
		return ret;
	}

	// �ر�Socket
	void Close()
	{
		if (INVALID_SOCKET != _sock)
		{
#ifdef _WIN32
			// 7 �ر�socket closesocket
			closesocket(_sock);
			// ���socket����
			WSACleanup();
#else
			close(_sock);
#endif
			printf("�ͻ���<Socket = %d>���˳�\n", _sock);
			_sock = INVALID_SOCKET;
		}
	}

	// ��������
	int SendData(DataHeader* header)
	{
		if (isRun() && header)
		{
			return (int)send(_sock, (const char*)header, (int)header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

	//�������� ����ճ�� �ٰ� ��ְ�
	int RecvData(SOCKET csock)
	{
		// 5 �ӷ��������
		int nLen = (int)recv(csock, _szRecv, RECV_BUFFER_SIZE, 0);
		if (nLen <= 0)
		{
			printf("<Socket = %d>��������Ͽ������������\n", csock);
			return -1;
		}
		//�����յ������ݿ�������Ϣ������
		memcpy(_szMsgBuf + _lastPos, _szRecv, nLen); //���ǵ�����һ��������Ϣ��ʱ��Ҫƫ�Ƶ�β��λ��
		//��Ϣ������������β��λ�ú���
		_lastPos += nLen;
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
		while (_lastPos >= sizeof(DataHeader)) // if������ٰ������ Ҫʹ��while���Խ��ճ�������
		{
			//��ʱ�Ϳ���֪����ǰ��Ϣ�ĳ���
			DataHeader* header = (DataHeader*)_szMsgBuf;
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (_lastPos >= header->dataLength)
			{
				//ʣ��δ������Ϣ�������ĳ���
				int nSize = _lastPos - header->dataLength;
				//����������Ϣ
				OnNetMsg(header);
				//��δ�������Ϣ����������ǰ��
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, nSize);
				//��Ϣ������������β��λ��ǰ��
				_lastPos = nSize;
			}
			else
			{
				//��Ϣ������ʣ�����ݲ���һ��������Ϣ
				break;
			}
		}
		return 0;
	}

	// ����������Ϣ
	virtual void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{
			/*LoginResult* loginResult = (LoginResult*)header;
			printf("<socket=%d>�յ��������Ϣ��CMD_LOGIN_RESULT ���ݳ��ȣ�%d��result = %d\n", _sock, loginResult->dataLength, loginResult->result);*/
		}
		break;
		case CMD_LOGINOUT_RESULT:
		{
			//LoginoutResult* logoutRet = (LoginoutResult*)header;
			//printf("<socket=%d>�յ��������Ϣ��CMD_LOGINOUT_RESULT ���ݳ��ȣ�%d��result = %d\n", _sock, logoutRet->dataLength, logoutRet->result);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			//NewUserJoin* newUserJoin = (NewUserJoin*)header;
			//printf("<socket=%d>�յ��������Ϣ��CMD_NEW_USER_JOIN ���ݳ��ȣ�%d�����û�����<Socket=%d>\n", _sock, newUserJoin->dataLength, newUserJoin->sock);
		}
		break;
		case CMD_ERROR:
		{
			printf("<socket=%d>�յ��������Ϣ��CMD_ERROR ���ݳ��ȣ�%d\n", _sock, header->dataLength);
		}
		break;
		default:
		{
			printf("<socket=%d>�յ�δ������Ϣ�����ݳ���%d\n", _sock, header->dataLength);
		}
		break;
		}
	}

	//��ѯ�Ƿ���������Ϣ
	bool onRun()
	{
		if (isRun())
		{
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(_sock, &readfds);
			timeval t = { 0, 0 };
			int ret = select(_sock + 1, &readfds, NULL, NULL, &t);
			//int ret = select(_sock, &readfds, NULL, NULL, NULL);
			if (ret < 0)
			{
				printf("<Socket=%d>select�������1��\n", _sock);
				return false;
			}
			if (FD_ISSET(_sock, &readfds))
			{
				FD_CLR(_sock, &readfds);
				if (-1 == RecvData(_sock))
				{
					printf("<Socket=%d>select�������2��\n", _sock);
					return false;
				}
			}
			return true;
		}
		return false;
	}

	bool isRun()
	{
		return INVALID_SOCKET != _sock;
	}

};

#endif // !_EasyTcpClient_hpp_
