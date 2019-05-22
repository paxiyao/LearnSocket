#include<iostream>
#include<string>
#include<string.h>
#include<stdio.h>
#include<vector>

using namespace std;

int main()
{
	string s1 = "123456";
	const char* c1 = s1.data();
	const char* c2 = s1.c_str();
	int len = s1.length();
	char ca[7];
	for (int i = 0; i < len; i++)
	{
		ca[i] = s1[i];
	}
	printf("%c\n", s1[0]);
	char* cb = new char[6];
	strcpy_s(cb, 7, "123456");
	string s2 = cb;
	*cb;

	//char* cc = new char[len + 1];

	/*字符数组赋初值*/
	char cArr[6] = { 'I', 'L', 'O', 'V', 'E', 'C' };
	/*字符串赋初值*/
	char sArr[7] = "ILOVEC";
	/*用sizeof（）求长度*/
	printf("cArr的长度=%d\n", sizeof(cArr));
	printf("sArr的长度=%d\n", sizeof(sArr));
	/*用strlen（）求长度*/
	printf("cArr的长度=%d\n", strlen(cArr));
	printf("sArr的长度=%d\n", strlen(sArr));
	/*用printf的%s打印内容*/
	printf("cArr的内容=%s\n", cArr);
	printf("sArr的内容=%s\n", sArr);

	vector<int> iv = { 1, 2, 2, 4, 5 };

	for (int i = (int)iv.size() - 1; i >= 0; i--)
	{
		if (2 == iv[i])
		{
			auto iter = iv.begin() + i; //std::vector<SOCKET>::iterator
			if (iv.end() != iter) 
			{
				iv.erase(iter);
			}
		}
	}

	vector<int>::iterator iter = iv.begin();
	for (; iter != iv.end();)
	{
		if (2 == *iter)
		{
			iter = iv.erase(iter);
		}
		else
		{
			iter++;
		}
	}

	return 0;
}