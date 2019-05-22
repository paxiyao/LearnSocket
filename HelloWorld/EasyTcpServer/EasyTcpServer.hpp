#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
	#define FD_SETSIZE      4096
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

#ifndef RECV_BUFFER_SIZE
#define RECV_BUFFER_SIZE 10240	// 缓存区最小单元的大小
#endif // !RECV_BUFFER_SIZE

#define CELLSERVER_THREAD_COUNT 4

#include<stdio.h>
#include<thread>
#include<vector>
#include<mutex>
#include<atomic>

#include "MessageHeader.hpp"
#include "CELLTimestamp.hpp"

class ClientSocket
{
private:
	SOCKET _sockfd;							//socket file descript 套接字文件描述符
	char _szMsgBuf[RECV_BUFFER_SIZE * 10];	//第二缓存区 消息缓冲区
	int _lastPos;							//消息缓冲区尾部的位置
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPos = 0;
	}

	SOCKET sockfd()
	{
		return _sockfd;
	}

	char* msgBuf()
	{
		return _szMsgBuf;
	}

	int getLastPos()
	{
		return _lastPos;
	}

	void setLastPos(int pos)
	{
		_lastPos = pos;
	}
};

class CellServer
{
public:
	CellServer(SOCKET sock = INVALID_SOCKET)
	{
		_sock = sock;
		memset(_szRecv, 0, RECV_BUFFER_SIZE);
		_pThread = nullptr;
		_recvCount = 0;
	}

	~CellServer()
	{
		Close();
		_sock = INVALID_SOCKET;
		delete _pThread;
		_pThread = nullptr;
	}

	bool isRun()
	{
		return INVALID_SOCKET != _sock;
	}

	//查询网络消息
	bool onRun()
	{
		while (isRun())
		{
			if (_clientBuffers.size() > 0)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientBuffers)
				{
					_clients.push_back(pClient);
				}
				_clientBuffers.clear();
			}

			//如果没有要处理的客户端就跳过
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}

			// 伯克利socket
			fd_set readfds;
			fd_set writefds;
			fd_set exceptfds;

			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&exceptfds);
			// 将描述符(socket)加入集合
			FD_SET(_sock, &readfds);
			FD_SET(_sock, &writefds);
			FD_SET(_sock, &exceptfds);

			// 将新加入的客户端放到可读集合里面去查询，看有没有数据需要接收
			// 使用减法不用每次都去调用_clients.size()
			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(_clients[n]->sockfd(), &readfds);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}

			/// nfds是一个整数值 是指fd_set集合中所有描述符的范围，而不是数量
			/// 既是所有文件描述符(Socket)最大值+1 在windows这个参数可以写0
			// select查询超时时间,最大查询等待时间，如果有数据会马上处理
			timeval t = { 1, 0 };
			int ret = select(maxSock + 1, &readfds, &writefds, &exceptfds, &t);
			if (ret < 0)
			{
				printf("select任务结束。\n");
				Close();
				return false;
			}
			// 判断描述符(socket)是否在集合中
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(_clients[n]->sockfd(), &readfds))
				{
					if (-1 == RecvData(_clients[n]))
					{
						auto iter = _clients.begin() + n; //vector<ClientSocket*>::iterator
						if (_clients.end() != iter)
						{
							delete _clients[n];
							_clients.erase(iter);
						}
					}
				}
			}
		}
		printf("退出了");
	}

	//接收客户端消息
	int RecvData(ClientSocket* pClient)
	{
		// 5 接受客户端数据
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFFER_SIZE, 0);
		if (nLen <= 0)
		{
			printf("客户端<Socket = %d>退出，任务结束。\n", pClient->sockfd());
			return -1;
		}
		//将接收到的数据拷贝到消息缓冲区
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//消息缓冲区的数据尾部位置后移
		pClient->setLastPos(pClient->getLastPos() + nLen);
		//判断消息缓冲区的数据长度大于消息头的长度
		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			//这时就可以知道当前消息的长度
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//判断消息缓冲区的数据长度大于消息长度
			if (pClient->getLastPos() >= header->dataLength)
			{
				//剩余未处理消息缓冲区的长度
				int nSize = pClient->getLastPos() - header->dataLength;
				//处理网络消息
				OnNetMsg(pClient->sockfd(), header);
				//将未处理的消息缓冲区数据前移
				memcpy(pClient->msgBuf() + header->dataLength, pClient->msgBuf(), nSize);
				//消息缓冲区的数据尾部位置前移
				pClient->setLastPos(nSize);
			}
			else
			{
				//消息缓冲区的剩余数据不够一个完整消息
				break;
			}
		}
		return 0;
	}

	//处理网络消息
	virtual void OnNetMsg(SOCKET csock, DataHeader* header)
	{
		_recvCount++;
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			//Login* login = (Login*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGIN 数据长度：%d，userName = %s，password = %s\n", csock, login->dataLength, login->userName, login->password);
			//LoginResult ret;
			//SendData(csock, &ret);
		}
		break;
		case CMD_LOGINOUT:
		{
			//Loginout* loginout = (Loginout*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGINOUT 数据长度：%d，userName = %s\n", csock, loginout->dataLength, loginout->userName);
			//LoginoutResult logoutRet;
			//SendData(csock, &logoutRet);
		}
		break;
		default:
		{
			//DataHeader hd;
			//SendData(csock, &hd);
		}
		break;
		}
	}

	//关闭Socket
	void Close()
	{
		if (INVALID_SOCKET != _sock)
		{
#ifdef _WIN32
			// 6 关闭socket closesocket
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			closesocket(_sock);
			_sock = INVALID_SOCKET;
			///
			WSACleanup();
#else
			// 6 关闭socket closesocket
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			close(_sock);
			_sock = INVALID_SOCKET;
#endif
			_clients.clear();
		}
	}

	void addClient(ClientSocket* pClient)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_clientBuffers.push_back(pClient);
	}

	void Start()
	{
		_pThread = new std::thread(std::mem_fun(&CellServer::onRun), this);
	}

	size_t getClientCount()
	{
		return _clients.size() + _clientBuffers.size();
	}
private:
	SOCKET _sock;
	//正式客户队列
	std::vector<ClientSocket*> _clients;		//这里要用指针不能用对象，栈的空间有限，要在堆上开辟空间
	//缓冲客户队列
	std::vector<ClientSocket*> _clientBuffers;
	char _szRecv[RECV_BUFFER_SIZE];				//接收缓冲区
	std::mutex _mutex;
	std::thread* _pThread;
public:
	std::atomic_int _recvCount;
};

class EasyTcpServer
{
private:
	SOCKET _sock;
	std::vector<ClientSocket*> _clients;		//这里要用指针不能用对象，栈的空间有限，要在堆上开辟空间
	std::vector<CellServer*> _cellServers;
	char _szRecv[RECV_BUFFER_SIZE];				//接收缓冲区
	CELLTimestamp _tTime;
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
		memset(_szRecv, 0, RECV_BUFFER_SIZE);
	}

	virtual ~EasyTcpServer()
	{
		Close();
	}

	//初始化Socket
	int	InitSocket()
	{
#ifdef _WIN32
		// 启动windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif

		if (INVALID_SOCKET != _sock)
		{
			printf("<Socket=%d>关闭旧连接...\n", _sock);
			Close();
		}
		///
		// 1 建立一个socket
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		return _sock;
	}
	//绑定端口
	void Bind(const char* ip, unsigned short port)
	{
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);//host to net unsigned short
#ifdef _WIN32
		if (ip)
		{
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else
		{
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}
#else
		if (ip)
		{
			_sin.sin_addr.s_addr = inet_addr(ip);
		}
		else
		{
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			printf("ERROR, 绑定网络端口<%u>失败...\n", port);
		}
		else
		{
			printf("绑定网络端口<%u>成功...\n", port);
		}
	}
	//监听端口
	void Listen(int n)
	{
		// 3 监听网络端口 listen
		if (SOCKET_ERROR == listen(_sock, n))//接收多少客户端连接
		{
			printf("错误, 监听网络端口失败...\n");
		}
		else
		{
			printf("监听网络端口成功...\n");
		}
	}
	//查询网络消息
	bool onRun()
	{
		if (isRun())
		{
			time4package();
			// 伯克利socket
			fd_set readfds;
			fd_set writefds;
			fd_set exceptfds;

			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&exceptfds);
			// 将描述符(socket)加入集合
			FD_SET(_sock, &readfds);
			FD_SET(_sock, &writefds);
			FD_SET(_sock, &exceptfds);

			// 将新加入的客户端放到可读集合里面去查询，看有没有数据需要接收
			/// nfds是一个整数值 是指fd_set集合中所有描述符的范围，而不是数量
			/// 既是所有文件描述符(Socket)最大值+1 在windows这个参数可以写0
			// select查询超时时间,最大查询等待时间，如果有数据会马上处理
			timeval t = { 1, 0 };
			int ret = select(_sock + 1, &readfds, &writefds, &exceptfds, &t);
			if (ret < 0)
			{
				printf("select任务结束。\n");
				Close();
				return false;
			}
			// 判断描述符(socket)是否在集合中
			if (FD_ISSET(_sock, &readfds))
			{
				// 可读集合中清理服务端socket
				FD_CLR(_sock, &readfds);
				Accept();
				return true;
			}
			return true;
		}
		return false;
	}

	bool isRun()
	{
		return INVALID_SOCKET != _sock;
	}

	//接收客户端
	SOCKET Accept()
	{
		// 4 等待接受客户端连接 accept
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET csock = INVALID_SOCKET;        //用来接受数据的变量进行初始化
#ifdef _WIN32
		csock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		csock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t *)&nAddrLen);
#endif
		if (INVALID_SOCKET == csock)
		{
			printf("<socket=%d>错误,接收到无效客户端socket...\n", (int)csock);
		}
		else
		{
			// 广播有新客户端加入
			//NewUserJoin userJoin;
			//SendDataToAll(&userJoin);
			addClientToCellServer(new ClientSocket(csock));
			//printf("<socket=%d>新客户端<%d>加入: socket = %d, IP = %s \n", (int)csock, _clients.size(), (int)csock, inet_ntoa(clientAddr.sin_addr));
		}
		return csock;
	}

	void addClientToCellServer(ClientSocket* pClient)
	{
		_clients.push_back(pClient);
		//查找客户数量最少的CellServer消息处理对象
		auto pMinServer = _cellServers[0];
		for (auto pCellServer : _cellServers)
		{
			if (pMinServer->getClientCount() > pCellServer->getClientCount())
			{
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);
	}

	void Start()
	{
		for (int i = 0; i < CELLSERVER_THREAD_COUNT; i++)
		{
			auto ser = new CellServer(_sock);
			_cellServers.push_back(ser);
			ser->Start();
		}
	}

	//处理网络消息
	virtual void time4package()
	{
		auto t = _tTime.getElapsedSecond();
		if (t >= 1.0)
		{
			int recvCount = 0;
			for (auto cer : _cellServers)
			{
				recvCount += cer->_recvCount;
				cer->_recvCount = 0;
			}
			printf("time<%lf>,socket<%d>,clients<%d>,recvCount<%d>\n", t, _sock, _clients.size(), recvCount);
			_tTime.update();
		}
	}

	// 发送指定socket数据
	int SendData(SOCKET csock, DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(csock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

	// 发送指定socket数据
	void SendDataToAll(DataHeader* header)
	{
		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{
			SendData(_clients[n]->sockfd(), header);
		}
	}

	//关闭Socket
	void Close()
	{
		if (INVALID_SOCKET != _sock)
		{
#ifdef _WIN32
			// 6 关闭socket closesocket
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			closesocket(_sock);
			_sock = INVALID_SOCKET;
			///
			WSACleanup();
#else
			// 6 关闭socket closesocket
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			close(_sock);
			_sock = INVALID_SOCKET;
#endif
			_clients.clear();
		}
	}
};

#endif // !_EasyTcpServer_hpp_
