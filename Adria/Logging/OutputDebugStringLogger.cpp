#include "OutputDebugStringLogger.h"
#include "Core/Windows.h"

namespace adria
{
	OutputDebugStringLogger::OutputDebugStringLogger(LogLevel logger_level /*= ELogLevel::LOG_DEBUG*/)
		: logger_level{ logger_level }
	{}

	OutputDebugStringLogger::~OutputDebugStringLogger()
	{}

	void OutputDebugStringLogger::Log(LogLevel level, char const* entry, char const* file, uint32_t line)
	{
		std::string log = GetLogTime() + LineInfoToString(file, line) + LevelToString(level) + std::string(entry) + "\n";
		OutputDebugStringA(log.c_str());
	}
}

