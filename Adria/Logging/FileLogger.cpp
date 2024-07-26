#include "FileLogger.h"
#include "Core/Paths.h"

namespace adria
{

	FileLogger::FileLogger(char const* log_file, LogLevel logger_level) 
		: log_stream{ paths::LogDir + log_file, std::ios::out }, logger_level{ logger_level }
	{}

	FileLogger::~FileLogger()
	{
		log_stream.close();
	}

	void FileLogger::Log(LogLevel level, char const* entry, char const* file, uint32_t line)
	{
		if (level < logger_level) return;
		log_stream << GetLogTime() + LineInfoToString(file, line) + LevelToString(level) + std::string(entry) << "\n";
	}

}

