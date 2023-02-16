#pragma once
#include <memory>
#include <vector>
#include <thread>
#include <string>
#include <fstream>
#include <source_location>
#include "../Utilities/ConcurrentQueue.h"

namespace adria
{

	enum class ELogLevel : uint8_t
	{
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARNING,
		LOG_ERROR
	};
	
	std::string LevelToString(ELogLevel type);
	std::string GetLogTime();
	std::string LineInfoToString(char const* file, uint32_t line);

	class ILogger
	{
	public:
		virtual ~ILogger() = default;
		virtual void Log(ELogLevel level, char const* entry, char const* file, uint32_t line) = 0;
	};

	class FileLogger : public ILogger
	{
	public:
		FileLogger(char const* log_file, ELogLevel logger_level = ELogLevel::LOG_DEBUG);
		virtual ~FileLogger() override;
		virtual void Log(ELogLevel level, char const* entry, char const* file, uint32_t line) override;
	private:
		std::ofstream log_stream;
		ELogLevel const logger_level;
	};

	class OutputStreamLogger : public ILogger
	{
	public:
		OutputStreamLogger(bool use_cerr = false, ELogLevel logger_level = ELogLevel::LOG_DEBUG);
		virtual ~OutputStreamLogger() override;
		virtual void Log(ELogLevel level, char const* entry, char const* file, uint32_t line) override;
	private:
		bool const use_cerr;
		ELogLevel const logger_level;
	};

	class OutputDebugStringLogger : public ILogger
	{
	public:
		OutputDebugStringLogger(ELogLevel logger_level = ELogLevel::LOG_DEBUG);
		virtual ~OutputDebugStringLogger() override;
		virtual void Log(ELogLevel level, char const* entry, char const* file, uint32_t line) override;
	private:
		ELogLevel const logger_level;
	};

	class LogManager
	{
		struct QueueEntry
		{
			ELogLevel level;
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
		void Log(ELogLevel level, char const* str, char const* file, uint32_t line);
		void Log(ELogLevel level, char const* str, std::source_location location = std::source_location::current());

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
	g_log.Log(ELogLevel::LOG_##level, buf.get(), __FILE__, __LINE__);  \
}()

}