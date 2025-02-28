#include <iostream>
#include "OutputStreamLogger.h"

namespace adria
{

	OutputStreamLogger::OutputStreamLogger(Bool use_cerr /*= false*/, LogLevel logger_level /*= ELogLevel::LOG_DEBUG*/)
		: use_cerr{ use_cerr }, logger_level{ logger_level } {}

	OutputStreamLogger::~OutputStreamLogger() {}

	void OutputStreamLogger::Log(LogLevel level, Char const* entry, Char const* file, uint32_t line)
	{
		if (level < logger_level) return;
		(use_cerr ? std::cerr : std::cout) << GetLogTime() + LineInfoToString(file, line) + LevelToString(level) + std::string(entry) << "\n";
	}

}

