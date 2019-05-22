#include<iostream>
#include<thread>
#include<mutex>
#include<atomic>
#include"CELLTimestamp.hpp"
using namespace std;
// 原子操作 不可分割的操作 计算机处理命令时最小的操作单位
mutex m;
const int tCount = 4;
atomic_int sum = 0;

void workFun(int index)
{
	for (int i = 0; i < 20000000; i++)
	{
		// 自解锁
		//lock_guard<mutex> lg(m);
		//m.lock();
		// 临界区域开始
		sum++;
		// 临界区域结束
		//m.unlock();
	}// 线程安全 线程不安全
}// 临界区域抢占资源

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