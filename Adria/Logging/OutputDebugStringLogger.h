#pragma once
#include "Logger.h"

namespace adria
{

	class OutputDebugStringLogger : public ILogger
	{
	public:
		OutputDebugStringLogger(LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~OutputDebugStringLogger() override;
		virtual void Log(LogLevel level, char const* entry, char const* file, uint32_t line) override;
	private:
		LogLevel const logger_level;
	};

}