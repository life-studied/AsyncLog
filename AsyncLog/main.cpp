// AsyncLog.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "AsyncLog.h"
#include <chrono>
#include <thread>
int main()
{
	AsyncLog::ELog("hello {}", 1);
	AsyncLog::WLog("hello {}", 1);
	AsyncLog::ILog("hello {}", 1);
	AsyncLog::ELog("hello {}", 1);
	AsyncLog::ELog("hello {}", 1);
	AsyncLog::ELog("hello {}", 1);
	std::this_thread::sleep_for(std::chrono::seconds(1));
}


