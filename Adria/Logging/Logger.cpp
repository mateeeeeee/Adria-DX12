#include "Logger.h"
#include <chrono>
#include <ctime>   
#include <iostream>

#include "../Core/Windows.h"
#include "../Core/Macros.h"

namespace adria
{

	std::string LevelToString(LogLevel type)
	{
		switch (type)
		{
		case LogLevel::LOG_DEBUG:
			return "[DEBUG]";
		case LogLevel::LOG_INFO:
			return "[INFO]";
		case LogLevel::LOG_WARNING:
			return "[WARNING]";
		case LogLevel::LOG_ERROR:
			return "[ERROR]";
		}
		return "";
	}
	std::string GetLogTime()
	{
		auto time = std::chrono::system_clock::now();
		time_t c_time = std::chrono::system_clock::to_time_t(time);
		std::string time_str = std::string(ctime(&c_time));
		time_str.pop_back();
		return "[" + time_str + "]";
	}
	std::string LineInfoToString(char const* file, uint32_t line)
	{
		return "[File: " + std::string(file) + "  Line: " + std::to_string(line) + "]";
	}

	LogManager::LogManager() : log_thread(&LogManager::ProcessLogs, this)
	{}

	LogManager::~LogManager()
	{
		exit.store(true);
		log_thread.join();
	}

	void LogManager::RegisterLogger(ILogger* logger)
	{
		loggers.push_back(logger);
	}

	void LogManager::Log(LogLevel level, char const* str, char const* filename, uint32_t line)
	{
		QueueEntry entry{ level, str, filename, line };
		log_queue.Push(std::move(entry));
	}

	void LogManager::Log(LogLevel level, char const* str, std::source_location location /*= std::source_location::current()*/)
	{
		Log(level, str, location.file_name(), location.line());
	}

	void LogManager::ProcessLogs()
	{
		QueueEntry entry{};
		while (true)
		{
			bool success = log_queue.TryPop(entry);
			if (success)
			{
				for (auto&& logger : loggers) if (logger) logger->Log(entry.level, entry.str.c_str(), entry.filename.c_str(), entry.line);
			}
			if (exit.load() && log_queue.Empty()) break;
		}
	}

	FileLogger::FileLogger(char const* log_file, LogLevel logger_level) : log_stream{ log_file, std::ios::out }, logger_level{ logger_level }
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

	OutputStreamLogger::OutputStreamLogger(bool use_cerr /*= false*/, LogLevel logger_level /*= ELogLevel::LOG_DEBUG*/)
		: use_cerr{ use_cerr }, logger_level{ logger_level }
	{
	}

	OutputStreamLogger::~OutputStreamLogger()
	{
	}

	void OutputStreamLogger::Log(LogLevel level, char const* entry, char const* file, uint32_t line)
	{
		if (level < logger_level) return;
		(use_cerr ? std::cerr : std::cout) << GetLogTime() + LineInfoToString(file, line) + LevelToString(level) + std::string(entry) << "\n";
	}

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