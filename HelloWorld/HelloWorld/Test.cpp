#define WIN32_LEAN_AND_MEAN  //减少早期winsock的宏引入

#include<WinSock2.h>
#include<windows.h>

//#pragma comment(lib, "ws2_32.lib")

//int main()
//{
//	WORD ver = MAKEWORD(2, 2);
//	WSADATA dat;
//	WSAStartup(ver, &dat);
//	///
//	///
//	WSACleanup();
//	return 0;
//}