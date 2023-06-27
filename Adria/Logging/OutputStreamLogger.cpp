#include "OutputStreamLogger.h"
#include <iostream>

namespace adria
{

	OutputStreamLogger::OutputStreamLogger(bool use_cerr /*= false*/, LogLevel logger_level /*= ELogLevel::LOG_DEBUG*/)
		: use_cerr{ use_cerr }, logger_level{ logger_level } {}

	OutputStreamLogger::~OutputStreamLogger() {}

	void OutputStreamLogger::Log(LogLevel level, char const* entry, char const* file, uint32_t line)
	{
		if (level < logger_level) return;
		(use_cerr ? std::cerr : std::cout) << GetLogTime() + LineInfoToString(file, line) + LevelToString(level) + std::string(entry) << "\n";
	}

}

