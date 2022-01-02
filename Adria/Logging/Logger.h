#pragma once
#include <memory>
#include <vector>
#include <thread>
#include <string>
#include <fstream>
#if defined(__cpp_lib_source_location)
#include <source_location>
#endif
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
		ELogLevel logger_level;
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
#if defined(__cpp_lib_source_location)
		void Log(ELogLevel level, char const* str, std::source_location location = std::source_location::current());
#endif

	private:
		std::vector<std::unique_ptr<ILogger>> loggers;
		std::thread log_thread;
		adria::ConcurrentQueue<QueueEntry> log_queue;
		std::atomic_bool exit = false;

	private:
		void ProcessLogs();
	};

	inline LogManager g_log{};

#define ADRIA_REGISTER_LOGGER(logger) g_log.RegisterLogger(logger)
#define ADRIA_LOG(level, ... ) [&]()  \
{ \
	const std::size_t size = snprintf(nullptr, 0, __VA_ARGS__) + 1; \
	std::unique_ptr<char[]> buf = std::make_unique<char[]>(size); \
	snprintf(buf.get(), size, __VA_ARGS__); \
	g_log.Log(ELogLevel::LOG_##level, buf.get(), __FILE__, __LINE__);  \
}()

}