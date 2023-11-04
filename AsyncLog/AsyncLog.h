#ifndef __ASYNC_LOG__
#define __ASYNC_LOG__

#include <thread>
#include <condition_variable>
#include <mutex>

#include <iostream>
#include <any>
#include <queue>
#include <sstream>

namespace AsyncLog {

	enum class LogLv 
	{
		DEBUGS = 0,
		INFO = 1,
		WARN = 2,
		ERRORS = 3,
	};

	std::ostream& operator<<(std::ostream& os, LogLv lv)
	{
		switch (lv)
		{
		case LogLv::DEBUGS:
			os << "[DEBUG]:";
			break;
		case LogLv::INFO:
			os << "[INFOS]:";
			break;
		case LogLv::WARN:
			os << "[WARNS]:";
			break;
		case LogLv::ERRORS:
			os << "[ERROR]:";
			break;
		default:
			break;
		}
		return os;
	}

	class LogTask {
	public:
		LogTask() = default;
		LogTask(const LogTask& src) :_level(src._level), _logdatas(src._logdatas) {}
		LogTask(LogTask&& src) noexcept :_level(src._level), _logdatas(std::move(src._logdatas)) {}
		LogLv _level;
		std::queue<std::any> _logdatas;
	};


	class AsyncLog {
	public:
		AsyncLog(const AsyncLog&) = delete;
		AsyncLog& operator =(const AsyncLog&) = delete;
		AsyncLog(AsyncLog&&) = delete;
		

		static AsyncLog& Instance() {
			static AsyncLog instance;
			return instance;
		}

		~AsyncLog() {
			Stop();
			workthread.join();
			std::cout << "exit success" << std::endl;
		}

		template<typename T>
		std::any  toAny(const T& value) {
			return std::any(value);
		}
#if !_HAS_CXX17
		//如果不支持C++17,可采用如下函数入队
		template<typename Arg, typename ...Args>
		void TaskEnque(std::shared_ptr<LogTask> task, Arg&& arg, Args&&... args) {
			task->_logdatas.push(std::any(arg));
			TaskEnque(task, std::forward<Args>(args)...);
		}

		template<typename Arg>
		void TaskEnque(std::shared_ptr<LogTask> task, Arg&& arg) {
			task->_logdatas.push(std::any(arg));
		}
#endif
		//可变参数列表，异步写
		template<typename...  Args>
		void AsyncWrite(LogLv level, Args&&... args) {
			auto task = std::make_shared<LogTask>();
#if _HAS_CXX17
			//折叠表达式依次将可变参数写入队列,需C++17版本支持
			(task->_logdatas.push(args), ...);
#else
			//如不支持C++17 请用这个版本入队
			TaskEnque(task, args...);
#endif
			task->_level = level;
			std::unique_lock<std::mutex> lock(_mtx);
			_queue.push(task);
			lock.unlock();
			// 通知等待的线程有新的任务可处理
			_empty_cond.notify_one();
		}

		void Stop() {
			_b_stop = true;
			_empty_cond.notify_one();
		}



	private:

		AsyncLog() :_b_stop(false) {
			workthread = std::thread([this]() {
				while(true) {
					std::unique_lock<std::mutex> lock(_mtx);
					if(_queue.empty()) _empty_cond.wait(lock);	//单个消费者，无需考虑虚假唤醒问题

					// Stop函数导致的唤醒，直接退出
					if (_b_stop && _queue.empty()) {
						return;
					}

					auto logtask = _queue.front();
					_queue.pop();
					lock.unlock();
					processTask(logtask);
				}
				});
		}

		bool convert2Str(const std::any& data, std::string& str) {
			std::ostringstream ss;
			if (data.type() == typeid(int)) {
				ss << std::any_cast<int>(data);
			}
			else if (data.type() == typeid(float)) {
				ss << std::any_cast<float>(data);
			}
			else if (data.type() == typeid(double)) {
				ss << std::any_cast<double>(data);
			}
			else if (data.type() == typeid(std::string)) {
				ss << std::any_cast<std::string>(data);
			}
			else if (data.type() == typeid(char*)) {
				ss << std::any_cast<char*>(data);
			}
			else if (data.type() == typeid(char const*)) {
				ss << std::any_cast<char const*>(data);
			}
			else {
				return false;
			}
			str = ss.str();
			return true;
		}

		void processTask(std::shared_ptr<LogTask> task) {
			std::cout << task->_level;

			if (task->_logdatas.empty()) {
				return;
			}
			// 队列首元素
			auto head = task->_logdatas.front();
			task->_logdatas.pop();

			std::string formatstr = "";
			bool bsuccess = convert2Str(head, formatstr);
			if (!bsuccess) {
				return;
			}

			while (!(task->_logdatas.empty())) {
				auto data = task->_logdatas.front();
				formatstr = formatString(formatstr, data);
				task->_logdatas.pop();
			}

			std::cout << formatstr << std::endl;
		}
		template<typename ...Args>
		std::string formatString(const std::string& format, Args... args) {
			std::string result = format;
			size_t pos = 0;
			//lambda表达式查找并替换字符串
			auto replacePlaceholder = [&](const std::string& placeholder, const std::any& replacement) {
				std::string str_replement = "";
				bool bsuccess = convert2Str(replacement, str_replement);
				if (!bsuccess) {
					return;
				}

				size_t placeholderPos = result.find(placeholder, pos);
				if (placeholderPos != std::string::npos) {
					result.replace(placeholderPos, placeholder.length(), str_replement);
					pos = placeholderPos + str_replement.length();
				}
				else {
					result = result + " " + str_replement;
				}
			};

			(replacePlaceholder("{}", args), ...);
			return result;
		}

		std::condition_variable _empty_cond;
		std::queue<std::shared_ptr<LogTask>>  _queue;
		bool _b_stop;
		std::mutex _mtx;
		std::thread  workthread;
	};


	template<typename ... Args>
	void   ELog(Args&&... args) {
		AsyncLog::Instance().AsyncWrite(LogLv::ERRORS, std::forward<Args>(args)...);
	}

	template<typename ... Args>
	void  DLog(Args&&... args) {
		AsyncLog::Instance().AsyncWrite(LogLv::DEBUGS, std::forward<Args>(args)...);
	}

	template<typename ... Args>
	void  ILog(Args&&... args) {
		AsyncLog::Instance().AsyncWrite(LogLv::INFO, std::forward<Args>(args)...);
	}

	template<typename ... Args>
	void  WLog(Args&&... args) {
		AsyncLog::Instance().AsyncWrite(LogLv::WARN, std::forward<Args>(args)...);
	}

}

#endif
