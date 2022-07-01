#include "CommandLineParser.h"
#include <string>
#include <vector>
#include <ranges>
#include "../Core/Macros.h"
#include "../Utilities/StringUtil.h"
#include "../Utilities/HashMap.h"

namespace adria
{
	namespace CommandLineArgs
	{
		namespace
		{
			HashMap<ECmdLineArg, bool> bool_args;
			HashMap<ECmdLineArg, int32> int_args;
			HashMap<ECmdLineArg, std::string> string_args;

			void FillDefaults()
			{
				bool_args[ECmdLineArg::DebugLayer] = false;
				bool_args[ECmdLineArg::GpuValidation] = false;
				bool_args[ECmdLineArg::DredDebug] = false;
				bool_args[ECmdLineArg::Vsync] = false;
				bool_args[ECmdLineArg::WindowMaximize] = false;

				int_args[ECmdLineArg::WindowWidth] = 1080;
				int_args[ECmdLineArg::WindowHeight] = 720;
				int_args[ECmdLineArg::LogLevel] = 1;
				
				string_args[ECmdLineArg::SceneFile] = "scene.json";
				string_args[ECmdLineArg::LogFile] = "adria.log";
				string_args[ECmdLineArg::WindowTitle] = "adria";
			}

			enum class ECmdLineType
			{
				Bool,
				Int,
				String
			};
			inline constexpr ECmdLineType GetType(ECmdLineArg arg)
			{
				switch (arg)
				{
					case ECmdLineArg::Vsync:
					case ECmdLineArg::DebugLayer:
					case ECmdLineArg::GpuValidation:
					case ECmdLineArg::DredDebug:
					case ECmdLineArg::WindowMaximize:
						return ECmdLineType::Bool;
					case ECmdLineArg::WindowWidth:
					case ECmdLineArg::WindowHeight:
					case ECmdLineArg::LogLevel:
						return ECmdLineType::Int;
					case ECmdLineArg::WindowTitle:
					case ECmdLineArg::SceneFile:
					case ECmdLineArg::LogFile:
						return ECmdLineType::String;
					case ECmdLineArg::Invalid:
					default:
						__assume(false); //std::unreachable() in C++23
				}
			}
			inline ECmdLineArg GetArg(std::string_view arg)
			{
				if (arg == "width" || arg == "w")		return ECmdLineArg::WindowWidth;
				if (arg == "max" || arg == "maximize")	return ECmdLineArg::WindowMaximize;
				if (arg == "height" || arg == "h")		return ECmdLineArg::WindowHeight;
				if (arg == "title")						return ECmdLineArg::WindowTitle;
				if (arg == "scenefile")					return ECmdLineArg::SceneFile;
				if (arg == "logfile")					return ECmdLineArg::LogFile;
				if (arg == "loglevel")					return ECmdLineArg::LogLevel;
				if (arg == "debuglayer")				return ECmdLineArg::DebugLayer;
				if (arg == "dreddebug")					return ECmdLineArg::DredDebug;
				if (arg == "gpuvalidation")				return ECmdLineArg::GpuValidation;
				if (arg == "vsync")						return ECmdLineArg::Vsync;
				return ECmdLineArg::Invalid;
			}
			inline bool ToBool(std::string_view arg)
			{
				if (arg == "true") return true;
				if (arg == "false") return false;
				ADRIA_ASSERT_MSG(false, "Invalid Bool Arg in Command Line!");
			}
			inline int32 ToInt(std::string_view arg)
			{
				return (int32)strtol(arg.data(), nullptr, 10);
			}
		}

		void Parse(wchar_t const* wide_cmd_line)
		{
			FillDefaults();
			std::string cmd_line = ConvertToNarrow(wide_cmd_line);
			constexpr std::string_view expression_delim{";"};
			for (std::string_view expression : std::views::split(cmd_line, expression_delim))
			{
				std::vector<std::string_view> operands;
				constexpr std::string_view operands_delim{"="};
				for (std::string_view operand : std::views::split(expression, operands_delim))
				{
					operands.push_back(operand);
				}
				if (operands.size() != 2) continue;

				std::string_view arg = operands[0];
				ECmdLineArg arg_ = GetArg(arg);
				ECmdLineType arg_type = GetType(arg_);
				switch (arg_type)
				{
				case ECmdLineType::Bool:
				{
					bool value = ToBool(operands[1]);
					bool_args[arg_] = value;
				}
				break;
				case ECmdLineType::Int:
				{
					int32 value = ToInt(operands[1]);
					int_args[arg_] = value;
				}
				break;
				case ECmdLineType::String:
				{
					string_args[arg_] = operands[1].data();
				}
				break;
				default:
					ADRIA_ASSERT(false);
				}
			}
		}
		bool GetBool(ECmdLineArg arg)
		{
			ADRIA_ASSERT(GetType(arg) == ECmdLineType::Bool);
			ADRIA_ASSERT(bool_args.contains(arg));
			return bool_args[arg];
		}
		int32 GetInt(ECmdLineArg arg)
		{
			ADRIA_ASSERT(GetType(arg) == ECmdLineType::Int);
			ADRIA_ASSERT(int_args.contains(arg));
			return int_args[arg];
		}
		char const* GetString(ECmdLineArg arg)
		{
			ADRIA_ASSERT(GetType(arg) == ECmdLineType::String);
			ADRIA_ASSERT(string_args.contains(arg));
			return string_args[arg].c_str();
		}

	}
}

