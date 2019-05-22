#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
	#define FD_SETSIZE      4096
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

#ifndef RECV_BUFFER_SIZE
#define RECV_BUFFER_SIZE 10240	// ��������С��Ԫ�Ĵ�С
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
	SOCKET _sockfd;							//socket file descript �׽����ļ�������
	char _szMsgBuf[RECV_BUFFER_SIZE * 10];	//�ڶ������� ��Ϣ������
	int _lastPos;							//��Ϣ������β����λ��
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

	//��ѯ������Ϣ
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

			//���û��Ҫ����Ŀͻ��˾�����
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}

			// ������socket
			fd_set readfds;
			fd_set writefds;
			fd_set exceptfds;

			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&exceptfds);
			// ��������(socket)���뼯��
			FD_SET(_sock, &readfds);
			FD_SET(_sock, &writefds);
			FD_SET(_sock, &exceptfds);

			// ���¼���Ŀͻ��˷ŵ��ɶ���������ȥ��ѯ������û��������Ҫ����
			// ʹ�ü�������ÿ�ζ�ȥ����_clients.size()
			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(_clients[n]->sockfd(), &readfds);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}

			/// nfds��һ������ֵ ��ָfd_set�����������������ķ�Χ������������
			/// ���������ļ�������(Socket)���ֵ+1 ��windows�����������д0
			// select��ѯ��ʱʱ��,����ѯ�ȴ�ʱ�䣬��������ݻ����ϴ���
			timeval t = { 1, 0 };
			int ret = select(maxSock + 1, &readfds, &writefds, &exceptfds, &t);
			if (ret < 0)
			{
				printf("select���������\n");
				Close();
				return false;
			}
			// �ж�������(socket)�Ƿ��ڼ�����
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
		printf("�˳���");
	}

	//���տͻ�����Ϣ
	int RecvData(ClientSocket* pClient)
	{
		// 5 ���ܿͻ�������
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFFER_SIZE, 0);
		if (nLen <= 0)
		{
			printf("�ͻ���<Socket = %d>�˳������������\n", pClient->sockfd());
			return -1;
		}
		//�����յ������ݿ�������Ϣ������
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//��Ϣ������������β��λ�ú���
		pClient->setLastPos(pClient->getLastPos() + nLen);
		//�ж���Ϣ�����������ݳ��ȴ�����Ϣͷ�ĳ���
		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			//��ʱ�Ϳ���֪����ǰ��Ϣ�ĳ���
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (pClient->getLastPos() >= header->dataLength)
			{
				//ʣ��δ������Ϣ�������ĳ���
				int nSize = pClient->getLastPos() - header->dataLength;
				//����������Ϣ
				OnNetMsg(pClient->sockfd(), header);
				//��δ�������Ϣ����������ǰ��
				memcpy(pClient->msgBuf() + header->dataLength, pClient->msgBuf(), nSize);
				//��Ϣ������������β��λ��ǰ��
				pClient->setLastPos(nSize);
			}
			else
			{
				//��Ϣ��������ʣ�����ݲ���һ��������Ϣ
				break;
			}
		}
		return 0;
	}

	//����������Ϣ
	virtual void OnNetMsg(SOCKET csock, DataHeader* header)
	{
		_recvCount++;
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			//Login* login = (Login*)header;
			//printf("�յ��ͻ���<Socket=%d>����CMD_LOGIN ���ݳ��ȣ�%d��userName = %s��password = %s\n", csock, login->dataLength, login->userName, login->password);
			//LoginResult ret;
			//SendData(csock, &ret);
		}
		break;
		case CMD_LOGINOUT:
		{
			//Loginout* loginout = (Loginout*)header;
			//printf("�յ��ͻ���<Socket=%d>����CMD_LOGINOUT ���ݳ��ȣ�%d��userName = %s\n", csock, loginout->dataLength, loginout->userName);
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

	//�ر�Socket
	void Close()
	{
		if (INVALID_SOCKET != _sock)
		{
#ifdef _WIN32
			// 6 �ر�socket closesocket
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
			// 6 �ر�socket closesocket
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
	//��ʽ�ͻ�����
	std::vector<ClientSocket*> _clients;		//����Ҫ��ָ�벻���ö���ջ�Ŀռ����ޣ�Ҫ�ڶ��Ͽ��ٿռ�
	//����ͻ�����
	std::vector<ClientSocket*> _clientBuffers;
	char _szRecv[RECV_BUFFER_SIZE];				//���ջ�����
	std::mutex _mutex;
	std::thread* _pThread;
public:
	std::atomic_int _recvCount;
};

class EasyTcpServer
{
private:
	SOCKET _sock;
	std::vector<ClientSocket*> _clients;		//����Ҫ��ָ�벻���ö���ջ�Ŀռ����ޣ�Ҫ�ڶ��Ͽ��ٿռ�
	std::vector<CellServer*> _cellServers;
	char _szRecv[RECV_BUFFER_SIZE];				//���ջ�����
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

	//��ʼ��Socket
	int	InitSocket()
	{
#ifdef _WIN32
		// ����windows socket 2.x����
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif

		if (INVALID_SOCKET != _sock)
		{
			printf("<Socket=%d>�رվ�����...\n", _sock);
			Close();
		}
		///
		// 1 ����һ��socket
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		return _sock;
	}
	//�󶨶˿�
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
			printf("ERROR, ������˿�<%u>ʧ��...\n", port);
		}
		else
		{
			printf("������˿�<%u>�ɹ�...\n", port);
		}
	}
	//�����˿�
	void Listen(int n)
	{
		// 3 ��������˿� listen
		if (SOCKET_ERROR == listen(_sock, n))//���ն��ٿͻ�������
		{
			printf("����, ��������˿�ʧ��...\n");
		}
		else
		{
			printf("��������˿ڳɹ�...\n");
		}
	}
	//��ѯ������Ϣ
	bool onRun()
	{
		if (isRun())
		{
			time4package();
			// ������socket
			fd_set readfds;
			fd_set writefds;
			fd_set exceptfds;

			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&exceptfds);
			// ��������(socket)���뼯��
			FD_SET(_sock, &readfds);
			FD_SET(_sock, &writefds);
			FD_SET(_sock, &exceptfds);

			// ���¼���Ŀͻ��˷ŵ��ɶ���������ȥ��ѯ������û��������Ҫ����
			/// nfds��һ������ֵ ��ָfd_set�����������������ķ�Χ������������
			/// ���������ļ�������(Socket)���ֵ+1 ��windows�����������д0
			// select��ѯ��ʱʱ��,����ѯ�ȴ�ʱ�䣬��������ݻ����ϴ���
			timeval t = { 1, 0 };
			int ret = select(_sock + 1, &readfds, &writefds, &exceptfds, &t);
			if (ret < 0)
			{
				printf("select���������\n");
				Close();
				return false;
			}
			// �ж�������(socket)�Ƿ��ڼ�����
			if (FD_ISSET(_sock, &readfds))
			{
				// �ɶ���������������socket
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

	//���տͻ���
	SOCKET Accept()
	{
		// 4 �ȴ����ܿͻ������� accept
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET csock = INVALID_SOCKET;        //�����������ݵı������г�ʼ��
#ifdef _WIN32
		csock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		csock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t *)&nAddrLen);
#endif
		if (INVALID_SOCKET == csock)
		{
			printf("<socket=%d>����,���յ���Ч�ͻ���socket...\n", (int)csock);
		}
		else
		{
			// �㲥���¿ͻ��˼���
			//NewUserJoin userJoin;
			//SendDataToAll(&userJoin);
			addClientToCellServer(new ClientSocket(csock));
			//printf("<socket=%d>�¿ͻ���<%d>����: socket = %d, IP = %s \n", (int)csock, _clients.size(), (int)csock, inet_ntoa(clientAddr.sin_addr));
		}
		return csock;
	}

	void addClientToCellServer(ClientSocket* pClient)
	{
		_clients.push_back(pClient);
		//���ҿͻ��������ٵ�CellServer��Ϣ�������
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

	//����������Ϣ
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

	// ����ָ��socket����
	int SendData(SOCKET csock, DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(csock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

	// ����ָ��socket����
	void SendDataToAll(DataHeader* header)
	{
		for (int n = (int)_clients.size() - 1; n >= 0; n--)
		{
			SendData(_clients[n]->sockfd(), header);
		}
	}

	//�ر�Socket
	void Close()
	{
		if (INVALID_SOCKET != _sock)
		{
#ifdef _WIN32
			// 6 �ر�socket closesocket
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
			// 6 �ر�socket closesocket
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
