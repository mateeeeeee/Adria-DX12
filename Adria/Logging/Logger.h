#pragma once
#include <memory> 
#include <string>
#include <source_location>

namespace adria
{

	enum class LogLevel : uint8
	{
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARNING,
		LOG_ERROR
	};

	std::string LevelToString(LogLevel type);
	std::string GetLogTime();
	std::string LineInfoToString(char const* file, uint32 line);

	class ILogger
	{
	public:
		virtual ~ILogger() = default;
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32 line) = 0;
	};

	class LogManager
	{
	public:
		LogManager();
		ADRIA_NONCOPYABLE_NONMOVABLE(LogManager)
		~LogManager();

		void Register(ILogger* logger);
		void Log(LogLevel level, char const* str, char const* file, uint32 line);
		void Log(LogLevel level, char const* str, std::source_location location = std::source_location::current());

	private:
		std::unique_ptr<class LogManagerImpl> pimpl;
	};
	inline LogManager g_Log{};

	#define ADRIA_LOG(level, ... ) [&]()  \
	{ \
		uint64 const size = snprintf(nullptr, 0, __VA_ARGS__) + 1; \
		std::unique_ptr<char[]> buf = std::make_unique<char[]>(size); \
		snprintf(buf.get(), size, __VA_ARGS__); \
		g_Log.Log(LogLevel::LOG_##level, buf.get(), __FILE__, __LINE__);  \
	}()

}