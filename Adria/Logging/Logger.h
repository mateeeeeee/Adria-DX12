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

}

#if defined(__cpp_lib_source_location) && defined(__cpp_lib_format)
#include <source_location>
#include <format>

inline void GLOBAL_LOG_INFO(std::string const& text, std::source_location location = std::source_location::current())
{
	adria::Log::Info(std::format("Function {} in file {} at line {}: {}", location.function_name(), location.file_name(), location.line(), text));
}
inline void GLOBAL_LOG_DEBUG(std::string const& text, std::source_location location = std::source_location::current())
{
	adria::Log::Debug(std::format("Function {} in file {} at line {}: {}", location.function_name(), location.file_name(), location.line(), text));
}
inline void GLOBAL_LOG_WARNING(std::string const& text, std::source_location location = std::source_location::current())
{
	adria::Log::Warning(std::format("Function {} in file {} at line {}: {}", location.function_name(), location.file_name(), location.line(), text));
}
inline void GLOBAL_LOG_ERROR(std::string const& text, std::source_location location = std::source_location::current())
{
	adria::Log::Error(std::format("Function {} in file {} at line {}: {}", location.function_name(), location.file_name(), location.line(), text));
}


inline void LOG_INFO(adria::Logger& logger, std::string const& text, std::source_location location = std::source_location::current())
{
	logger.Info(std::format("Function {} in file {} at line {}: {}", location.function_name(), location.file_name(), location.line(), text));
}
inline void LOG_DEBUG(adria::Logger& logger, std::string const& text, std::source_location location = std::source_location::current())
{
	logger.Debug(std::format("Function {} in file {} at line {}: {}", location.function_name(), location.file_name(), location.line(), text));
}
inline void LOG_WARNING(adria::Logger& logger, std::string const& text, std::source_location location = std::source_location::current())
{
	logger.Warning(std::format("Function {} in file {} at line {}: {}", location.function_name(), location.file_name(), location.line(), text));
}
inline void LOG_ERROR(adria::Logger& logger, std::string const& text, std::source_location location = std::source_location::current())
{
	logger.Error(std::format("Function {} in file {} at line {}: {}", location.function_name(), location.file_name(), location.line(), text));
}


#else 

template<typename... Args>
std::string StringFormat(std::string const& format, Args... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
	if (size <= 0) return "";
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args...);
	return std::string(buf.get(), buf.get() + size - 1);
}

#define GLOBAL_LOG_INFO(text)        { adria::Log::Info(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text));	}
#define GLOBAL_LOG_DEBUG(text)       { adria::Log::Debug(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text));	}
#define GLOBAL_LOG_WARNING(text)     { adria::Log::Warning(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__)  + ": " + std::string(text)); }
#define GLOBAL_LOG_ERROR(text)       { adria::Log::Error(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text)); }

#define LOG_INFO(logger,text)		 { logger.Info(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text));	}
#define LOG_DEBUG(logger,text)       { logger.Debug(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text));	}
#define LOG_WARNING(logger,text)     { logger.Warning(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__)  + ": " + std::string(text)); }
#define LOG_ERROR(logger,text)       { logger.Error(StringFormat("Function %s in file %s at line %d", __FUNCTION__,__FILE__, __LINE__) + ": " + std::string(text)); }


#endif

