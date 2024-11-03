#pragma once
#include <fstream>
#include "Logger.h"

namespace adria
{
	class FileLogger : public ILogger
	{
	public:
		FileLogger(Char const* log_file, LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~FileLogger() override;
		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;
	private:
		std::ofstream log_stream;
		LogLevel const logger_level;
	};

}