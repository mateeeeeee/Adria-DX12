#pragma once
#include "Logger.h"

namespace adria
{
	class OutputStreamLogger : public ILogger
	{
	public:
		OutputStreamLogger(bool use_cerr = false, LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~OutputStreamLogger() override;
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32 line) override;
	private:
		bool const use_cerr;
		LogLevel const logger_level;
	};

}