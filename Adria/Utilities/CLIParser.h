#pragma once
#include <shellapi.h>
#include <vector>
#include <string>
#include "StringUtil.h"

namespace adria
{
	class CLIArg
	{
		friend class CLIParser;
	public:
		CLIArg(std::vector<std::string>&& prefixes, bool has_value)
			: prefixes(std::move(prefixes)), has_value(has_value)
		{}

		bool AsBool(bool default_value = false) const
		{
			ADRIA_ASSERT(has_value);
			if (value == "true" || value == "1") return true;
			if (value == "false" || value == "0") return false;
			ADRIA_ASSERT_MSG(false, "Invalid bool argument!");
			ADRIA_UNREACHABLE();
		}
		bool AsBoolOr(bool def) const
		{
			if (IsPresent()) return AsBool();
			else return def;
		}

		int32 AsInt() const
		{
			ADRIA_ASSERT(has_value);
			return (int32)strtol(value.c_str(), nullptr, 10);
		}
		int32 AsIntOr(int32 def) const
		{
			if (IsPresent()) return AsInt();
			else return def;
		}

		float AsFloat() const
		{
			ADRIA_ASSERT(has_value);
			return (float)std::strtod(value.c_str(), nullptr);
		}
		float AsFloatOr(float def) const
		{
			if (IsPresent()) return AsFloat();
			else return def;
		}
		
		std::string AsString() const
		{
			ADRIA_ASSERT(has_value);
			return value;
		}
		std::string AsStringOr(std::string const& def) const
		{
			if (IsPresent()) return AsString();
			else return def;
		}

		bool IsPresent() const
		{
			return is_present;
		}
		operator bool() const
		{
			return IsPresent();
		}

	private:
		std::vector<std::string> prefixes;
		bool has_value;
		std::string value;
		bool is_present = false;

		void SetValue(std::string const& _value)
		{
			ADRIA_ASSERT(has_value);
			value = _value;
		}
		void SetIsPresent()
		{
			is_present = true;
		}
	};

	class CLIParser
	{
	public:
		CLIParser() 
		{
			args.reserve(128);
		}

		ADRIA_NODISCARD CLIArg& AddArg(bool has_value, std::convertible_to<std::string> auto... prefixes)
		{
			args.emplace_back(std::vector<std::string>{prefixes...}, has_value);
			return args.back();
		}

		void Parse(int argc, wchar_t** argv)
		{
			std::vector<std::wstring> cmdline(argv, argv + argc);
			for (uint64 i = 0; i < cmdline.size(); ++i)
			{
				bool found = false;
				for (CLIArg& arg : args)
				{
					for (auto const& prefix : arg.prefixes) 
					{
						bool prefix_found = cmdline[i] == ToWideString(prefix);
						if (prefix_found)
						{
							found = true;
							arg.SetIsPresent();
							if (arg.has_value) arg.SetValue(ToString(cmdline[++i]));
							break;
						}
					}
					if (found) break;
				}
			}
			LocalFree(argv);
		}

		void Parse(std::wstring const& cmd_line)
		{
			int argc;
			wchar_t** argv = CommandLineToArgvW(cmd_line.c_str(), &argc);
			Parse(argc, argv);
		}

	private:
		std::vector<CLIArg> args;
	};
}
