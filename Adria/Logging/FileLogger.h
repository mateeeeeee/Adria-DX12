#pragma once
#include <fstream>
#include "Logger.h"

namespace adria
{
	class FileLogger : public ILogger
	{
	public:
		FileLogger(char const* log_file, LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~FileLogger() override;
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32 line) override;
	private:
		std::ofstream log_stream;
		LogLevel const logger_level;
	};

}