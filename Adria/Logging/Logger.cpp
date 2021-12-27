#include "Logger.h"
#include <cassert>
#include <fstream>
#include <chrono>
#include <ctime>   
#include "../Core/Macros.h"

namespace adria
{


	LogType operator|(LogType lhs, LogType rhs)
	{
		using T = std::underlying_type_t<LogType>;
		return static_cast<LogType>(static_cast<T>(lhs) | static_cast<T>(rhs));
	}

	LogType& operator|=(LogType& lhs, LogType& rhs)
	{
		lhs = lhs | rhs;
		return lhs;
	}

	LogType operator&(LogType lhs, LogType rhs)
	{
		using T = std::underlying_type_t<LogType>;
		return static_cast<LogType>(static_cast<T>(lhs) & static_cast<T>(rhs));
	}

	std::string Logger::ToString(LogType type)
	{
		switch (type)
		{
		case LogType::DEBUG_LOG:
			return "DEBUG: ";
		case LogType::INFO_LOG:
			return "INFO: ";
		case LogType::WARNING_LOG:
			return "WARNING: ";
		case LogType::ERROR_LOG:
			return "ERROR: ";
		default:
			assert(false && "Invalid Log Type");
		}
		return "";
	}

	Logger::Logger(std::string_view log_file) : filter(LogType::INFO_LOG | LogType::DEBUG_LOG | LogType::ERROR_LOG | LogType::WARNING_LOG)
	{
		log_thread = std::thread{ &Logger::ProcessLogs, this, log_file };
	}

	Logger::Logger(std::string_view log_file, LogType filter) : filter(filter)
	{
		log_thread = std::thread{ &Logger::ProcessLogs, this, log_file };
	}

	Logger::~Logger()
	{
		exit.store(true);
		log_thread.join();
	}

	void Logger::Debug(std::string const& entry)
	{
		Log(entry, LogType::DEBUG_LOG);
	}

	void Logger::Info(std::string const& entry)
	{
		Log(entry, LogType::INFO_LOG);
	}

	void Logger::Warning(std::string const& entry)
	{
		Log(entry, LogType::WARNING_LOG);
	}

	void Logger::Error(std::string const& entry)
	{
		Log(entry, LogType::ERROR_LOG);
	}

	void Logger::AddLogCallback(std::function<void(std::string const&)> callback)
	{
		callbacks.push_back(callback);
	}

	void Logger::Log(std::string const& entry, LogType log_type)
	{
		
		if ((log_type & filter) != LogType::EMPTY_LOG)
		{
			auto time = std::chrono::system_clock::now();
			time_t c_time = std::chrono::system_clock::to_time_t(time);
			std::string time_str = std::string(ctime(&c_time));
			time_str.pop_back();
			std::string _entry = "[" + time_str + "] " + ToString(log_type) + entry;
			log_queue.Push(_entry);

			for (auto& callback : callbacks) callback(_entry);
		}
	}

	void Logger::ProcessLogs(std::string_view log_file)
	{
		std::ofstream log_stream{ log_file.data(), std::ios::out}; // | std::ios::app 
		assert(!log_stream.fail() && "Logger error");

		std::string entry{};
		while (true)
		{
			bool success = log_queue.TryPop(entry);
			if(success) log_stream << entry;

			if (exit.load() && log_queue.Empty()) break;
		}
	}


	namespace Log
	{
		std::unique_ptr<Logger> p_logger = nullptr;
		bool initialized = false;

		void Initialize(std::string_view log_file)
		{
			if(!initialized) p_logger = std::make_unique<Logger>(log_file);
			initialized = true;
		}

		void Initialize(std::string_view log_file, LogType filter)
		{
			if (!initialized) p_logger = std::make_unique<Logger>(log_file, filter);
			initialized = true;
		}

		void Destroy()
		{
			p_logger.reset(nullptr);
			initialized = false;
		}

		void Debug(std::string const& entry)
		{
			assert(initialized && "Logger was not initialized! Call Log::Initialize!");
			p_logger->Debug(entry);
		}

		void Info(std::string const& entry)
		{
			assert(initialized && "Logger was not initialized! Call Log::Initialize!");
			p_logger->Info(entry);
		}

		void Warning(std::string const& entry)
		{
			assert(initialized && "Logger was not initialized! Call Log::Initialize!");
			p_logger->Warning(entry);
		}

		void Error(std::string const& entry)
		{
			assert(initialized && "Logger was not initialized! Call Log::Initialize!");
			p_logger->Error(entry);
		}
		void AddLogCallback(std::function<void(std::string const&)> callback)
		{
			assert(initialized && "Logger was not initialized! Call Log::Initialize!");
			p_logger->AddLogCallback(callback);
		}
	}


}