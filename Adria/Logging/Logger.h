#pragma once
#include <memory>
#include <vector>
#include <thread>
#include <string>
#include <fstream>
#include <source_location>
#include "Utilities/ConcurrentQueue.h"

namespace adria
{

	enum class LogLevel : uint8_t
	{
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARNING,
		LOG_ERROR
	};
	
	std::string LevelToString(LogLevel type);
	std::string GetLogTime();
	std::string LineInfoToString(char const* file, uint32_t line);

	class ILogger
	{
	public:
		virtual ~ILogger() = default;
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32_t line) = 0;
	};

	class FileLogger : public ILogger
	{
	public:
		FileLogger(char const* log_file, LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~FileLogger() override;
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32_t line) override;
	private:
		std::ofstream log_stream;
		LogLevel const logger_level;
	};

	class OutputStreamLogger : public ILogger
	{
	public:
		OutputStreamLogger(bool use_cerr = false, LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~OutputStreamLogger() override;
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32_t line) override;
	private:
		bool const use_cerr;
		LogLevel const logger_level;
	};

	class OutputDebugStringLogger : public ILogger
	{
	public:
		OutputDebugStringLogger(LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~OutputDebugStringLogger() override;
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32_t line) override;
	private:
		LogLevel const logger_level;
	};

	class LogManager
	{
		struct QueueEntry
		{
			LogLevel level;
			std::string str;
			std::string filename;
			uint32_t line;
		};
	public:
		LogManager();
		LogManager(LogManager const&) = delete;
		LogManager& operator=(LogManager const&) = delete;
		LogManager(LogManager&&) = delete;
		LogManager& operator=(LogManager&&) = delete;
		~LogManager();

		void RegisterLogger(ILogger* logger);
		void Log(LogLevel level, char const* str, char const* file, uint32_t line);
		void Log(LogLevel level, char const* str, std::source_location location = std::source_location::current());

	private:
		std::vector<ILogger*> loggers;
		std::thread log_thread;
		ConcurrentQueue<QueueEntry> log_queue;
		std::atomic_bool exit = false;

	private:
		void ProcessLogs();
	};

	inline LogManager g_log{};

#define ADRIA_REGISTER_LOGGER(logger) g_log.RegisterLogger(logger)
#define ADRIA_LOG(level, ... ) [&]()  \
{ \
	size_t const size = snprintf(nullptr, 0, __VA_ARGS__) + 1; \
	std::unique_ptr<char[]> buf = std::make_unique<char[]>(size); \
	snprintf(buf.get(), size, __VA_ARGS__); \
	g_log.Log(LogLevel::LOG_##level, buf.get(), __FILE__, __LINE__);  \
}()

}