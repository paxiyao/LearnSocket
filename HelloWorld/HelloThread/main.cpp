#include<iostream>
#include<thread>
#include<mutex>
#include<atomic>
#include"CELLTimestamp.hpp"
using namespace std;
// ԭ�Ӳ��� ���ɷָ�Ĳ��� �������������ʱ��С�Ĳ�����λ
mutex m;
const int tCount = 4;
atomic_int sum = 0;

void workFun(int index)
{
	for (int i = 0; i < 20000000; i++)
	{
		// �Խ���
		//lock_guard<mutex> lg(m);
		//m.lock();
		// �ٽ�����ʼ
		sum++;
		// �ٽ��������
		//m.unlock();
	}// �̰߳�ȫ �̲߳���ȫ
}// �ٽ�������ռ��Դ

int main()
{
	thread t[tCount];
	for (int i = 0; i < tCount; i++)
	{
		t[i] = thread(workFun, i);
	}
	CELLTimestamp tTime;
	for (int i = 0; i < tCount; i++)
	{
		t[i].join();
	}
	cout << tTime.getElapsedTimeInMilliSec() << "sum:" << sum << endl;
	tTime.update();
	for (int i = 0; i < 80000000; i++)
	{
		sum++;
	}
	cout << tTime.getElapsedTimeInMilliSec() << "sum:" << sum << endl;
	cout << "Hello, main thread." << endl;
	//getchar();
	return 0;
}