#pragma once
#include "Logger.h"

namespace adria
{
	class OutputStreamLogger : public ILogger
	{
	public:
		OutputStreamLogger(Bool use_cerr = false, LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~OutputStreamLogger() override;
		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;
	private:
		Bool const use_cerr;
		LogLevel const logger_level;
	};

}