#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  //减少早期winsock的宏引入
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
#define RECV_BUFFER_SIZE 10240    // 缓存区最小单元的大小
#endif // !RECV_BUFFER_SIZE

class EasyTcpClient
{
private:
	SOCKET _sock;
	char _szRecv[RECV_BUFFER_SIZE];          //接收缓存区
	char _szMsgBuf[RECV_BUFFER_SIZE * 10];    //第二缓存区 消息缓冲区
	int _lastPos;                            //消息缓冲区尾部的位置
public:
	EasyTcpClient()
	{
		_sock = INVALID_SOCKET;
		memset(_szRecv, 0, RECV_BUFFER_SIZE);
		memset(_szMsgBuf, 0, RECV_BUFFER_SIZE * 10);
		_lastPos = 0;
	}

	// 虚析构函数
	virtual ~EasyTcpClient()
	{
		Close();
	}

	// 初始化socket
	int InitSocket()
	{
#ifdef _WIN32
		// 启动window socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			printf("<Socket=%d>关闭旧连接...\n", _sock);
			Close();
		}
		// 1 建立一个 socket
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("错误,建立Socket失败...\n");
		}
		else
		{
			//printf("<socket=%d>建立socket成功...\n", _sock);
		}
		return _sock;
	}

	// 连接服务器
	int Connect(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}
		// 2 连接服务器 connect
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
			printf("错误,连接Socket失败...\n");
		}
		else
		{
			//printf("<socket=%d>连接socket成功...\n", _sock);
		}
		return ret;
	}

	// 关闭Socket
	void Close()
	{
		if (INVALID_SOCKET != _sock)
		{
#ifdef _WIN32
			// 7 关闭socket closesocket
			closesocket(_sock);
			// 清除socket环境
			WSACleanup();
#else
			close(_sock);
#endif
			printf("客户端<Socket = %d>已退出\n", _sock);
			_sock = INVALID_SOCKET;
		}
	}

	// 发送数据
	int SendData(DataHeader* header)
	{
		if (isRun() && header)
		{
			return (int)send(_sock, (const char*)header, (int)header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

	//接收数据 处理粘包 少包 拆分包
	int RecvData(SOCKET csock)
	{
		// 5 接服务端数据
		int nLen = (int)recv(csock, _szRecv, RECV_BUFFER_SIZE, 0);
		if (nLen <= 0)
		{
			printf("<Socket = %d>与服务器断开，任务结束。\n", csock);
			return -1;
		}
		//将接收到的数据拷贝到消息缓冲区
		memcpy(_szMsgBuf + _lastPos, _szRecv, nLen); //考虑到不够一个完整消息的时候要偏移到尾部位置
		//消息缓存区的数据尾部位置后移
		_lastPos += nLen;
		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while (_lastPos >= sizeof(DataHeader)) // if解决了少包的情况 要使用while可以解决粘包的情况
		{
			//这时就可以知道当前消息的长度
			DataHeader* header = (DataHeader*)_szMsgBuf;
			//判断消息缓冲区的数据长度大于消息长度
			if (_lastPos >= header->dataLength)
			{
				//剩余未处理消息缓冲区的长度
				int nSize = _lastPos - header->dataLength;
				//处理网络消息
				OnNetMsg(header);
				//将未处理的消息缓冲区数据前移
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				_lastPos = nSize;
			}
			else
			{
				//消息缓冲区剩余数据不够一个完整消息
				break;
			}
		}
		return 0;
	}

	// 处理网络消息
	virtual void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{
			/*LoginResult* loginResult = (LoginResult*)header;
			printf("<socket=%d>收到服务端消息：CMD_LOGIN_RESULT 数据长度：%d，result = %d\n", _sock, loginResult->dataLength, loginResult->result);*/
		}
		break;
		case CMD_LOGINOUT_RESULT:
		{
			//LoginoutResult* logoutRet = (LoginoutResult*)header;
			//printf("<socket=%d>收到服务端消息：CMD_LOGINOUT_RESULT 数据长度：%d，result = %d\n", _sock, logoutRet->dataLength, logoutRet->result);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			//NewUserJoin* newUserJoin = (NewUserJoin*)header;
			//printf("<socket=%d>收到服务端消息：CMD_NEW_USER_JOIN 数据长度：%d，新用户加入<Socket=%d>\n", _sock, newUserJoin->dataLength, newUserJoin->sock);
		}
		break;
		case CMD_ERROR:
		{
			printf("<socket=%d>收到服务端消息：CMD_ERROR 数据长度：%d\n", _sock, header->dataLength);
		}
		break;
		default:
		{
			printf("<socket=%d>收到未定义消息，数据长度%d\n", _sock, header->dataLength);
		}
		break;
		}
	}

	//查询是否有网络消息
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
				printf("<Socket=%d>select任务结束1。\n", _sock);
				return false;
			}
			if (FD_ISSET(_sock, &readfds))
			{
				FD_CLR(_sock, &readfds);
				if (-1 == RecvData(_sock))
				{
					printf("<Socket=%d>select任务结束2。\n", _sock);
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
