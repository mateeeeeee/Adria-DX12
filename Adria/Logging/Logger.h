#pragma once
#include <memory> 
#include <string>
#include <source_location>

namespace adria
{

	enum class LogLevel : Uint8
	{
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARNING,
		LOG_ERROR
	};

	std::string LevelToString(LogLevel type);
	std::string GetLogTime();
	std::string LineInfoToString(Char const* file, Uint32 line);

	class ILogger
	{
	public:
		virtual ~ILogger() = default;
		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) = 0;
	};

	class LogManager
	{
	public:
		LogManager();
		ADRIA_NONCOPYABLE_NONMOVABLE(LogManager)
		~LogManager();

		void Register(ILogger* logger);
		void Log(LogLevel level, Char const* str, Char const* file, Uint32 line);
		void Log(LogLevel level, Char const* str, std::source_location location = std::source_location::current());

	private:
		std::unique_ptr<class LogManagerImpl> pimpl;
	};
	inline LogManager g_Log{};

	#define ADRIA_LOG(level, ... ) [&]()  \
	{ \
		Uint64 const size = snprintf(nullptr, 0, __VA_ARGS__) + 1; \
		std::unique_ptr<Char[]> buf = std::make_unique<Char[]>(size); \
		snprintf(buf.get(), size, __VA_ARGS__); \
		g_Log.Log(LogLevel::LOG_##level, buf.get(), __FILE__, __LINE__);  \
	}()
	#define ADRIA_DEBUG(...)	ADRIA_LOG(DEBUG, __VA_ARGS__)
	#define ADRIA_INFO(...)		ADRIA_LOG(INFO, __VA_ARGS__)
	#define ADRIA_WARNING(...)  ADRIA_LOG(WARNING, __VA_ARGS__)
	#define ADRIA_ERROR(...)	ADRIA_LOG(ERROR, __VA_ARGS__)
}