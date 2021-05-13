#pragma once
#include <thread>
#include <string_view>
#include "../Utilities/ConcurrentQueue.h"
#include <functional>

namespace adria
{

	enum class LogType : uint8_t
	{
		EMPTY_LOG = 0x00,
		DEBUG_LOG = 0x01,
		INFO_LOG = 0x02,
		WARNING_LOG = 0x04,
		ERROR_LOG = 0x08
	};


	LogType operator | (LogType, LogType);
	LogType& operator |= (LogType&, LogType&);
	LogType operator & (LogType, LogType);

	class Logger
	{
		static std::string ToString(LogType type);

	public:
		Logger(std::string_view log_file);

		Logger(std::string_view log_file, LogType filter);

		Logger(Logger const&) = delete;
		Logger& operator=(Logger const&) = delete;

		Logger(Logger&&) = delete;
		Logger& operator=(Logger&&) = delete;

		~Logger();

		void Debug(std::string const& entry);

		void Info(std::string const& entry);

		void Warning(std::string const& entry);

		void Error(std::string const& entry);

		void AddLogCallback(std::function<void(std::string const&)> callback);

	private:
		std::thread log_thread;
		ConcurrentQueue<std::string> log_queue;
		std::atomic_bool exit = false;
		LogType filter;

		std::vector<std::function<void(std::string const&)>> callbacks;

	private:

		void Log(std::string const& entry, LogType log_type);

		void ProcessLogs(std::string_view log_file);
	};

	//global logger
	namespace Log
	{
		void Initialize(std::string_view log_file);

		void Initialize(std::string_view log_file, LogType filter);

		void Destroy();

		void Debug(std::string const& entry);

		void Info(std::string const& entry);

		void Warning(std::string const& entry);

		void Error(std::string const& entry);

		void AddLogCallback(std::function<void(std::string const&)> callback);

	}


	//add std::format when MSVC++ implements it
	template<typename... Args>
	std::string StringFormat(std::string const& format, Args... args)
	{
		size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1; 
		if (size <= 0) return "";
		std::unique_ptr<char[]> buf(new char[size]);
		snprintf(buf.get(), size, format.c_str(), args...);
		return std::string(buf.get(), buf.get() + size - 1); 
	}
}

//add std::source_location when MSVC++ implements it
#define GLOBAL_LOG_INFO(text)        { Log::Info(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text));	}
#define GLOBAL_LOG_DEBUG(text)       { Log::Debug(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text));	}
#define GLOBAL_LOG_WARNING(text)     { Log::Warning(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__)  + ": " + std::string(text)); }
#define GLOBAL_LOG_ERROR(text)       { Log::Error(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text)); }

#define LOG_INFO(logger,text)		 { logger.Info(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text));	}
#define LOG_DEBUG(logger,text)       { logger.Debug(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text));	}
#define LOG_WARNING(logger,text)     { logger.Warning(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__)  + ": " + std::string(text)); }
#define LOG_ERROR(logger,text)       { logger.Error(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text)); }

