#pragma once
#include <shellapi.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "StringUtil.h"

namespace adria
{
	class CLIArg
	{
		friend class CLIParser;
	public:
		CLIArg(std::vector<std::string>&& prefixes, Bool has_value)
			: prefixes(std::move(prefixes)), has_value(has_value)
		{}

		Bool AsBool(Bool default_value = false) const
		{
			ADRIA_ASSERT(has_value);
			if (value == "true" || value == "1") return true;
			if (value == "false" || value == "0") return false;
			ADRIA_ASSERT_MSG(false, "Invalid bool argument!");
			ADRIA_UNREACHABLE();
		}
		Bool AsBoolOr(Bool def) const
		{
			if (IsPresent()) return AsBool();
			else return def;
		}

		Int AsInt() const
		{
			ADRIA_ASSERT(has_value);
			return (Int)strtol(value.c_str(), nullptr, 10);
		}
		Int AsIntOr(Int def) const
		{
			if (IsPresent()) return AsInt();
			else return def;
		}

		Float AsFloat() const
		{
			ADRIA_ASSERT(has_value);
			return (Float)std::strtod(value.c_str(), nullptr);
		}
		Float AsFloatOr(Float def) const
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

		Bool IsPresent() const
		{
			return is_present;
		}
		operator Bool() const
		{
			return IsPresent();
		}

	private:
		std::vector<std::string> prefixes;
		Bool has_value;
		std::string value;
		Bool is_present = false;

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
		CLIParser() {}

		ADRIA_MAYBE_UNUSED Uint32 AddArg(Bool has_value, std::convertible_to<std::string> auto... prefixes)
		{
			Uint32 const arg_index = (Uint32)args.size();
			CLIArg& arg = args.emplace_back(std::vector<std::string>{prefixes...}, has_value);
			for (std::string const& prefix : arg.prefixes)
			{
				prefix_arg_index_map[prefix] = arg_index;
			}
			return arg_index;
		}

		CLIArg const& operator[](std::convertible_to<std::string> auto const& prefix) const
		{
			ADRIA_ASSERT_MSG(prefix_arg_index_map.contains(prefix), "Did you forgot to add this Arg to CLIParser?");
			return args[prefix_arg_index_map[prefix]];
		}
		CLIArg const& operator[](Uint32 i) const
		{
			return args[i];
		}
		
		void Parse(Int argc, Wchar** argv)
		{
			std::vector<std::wstring> cmdline(argv, argv + argc);
			for (Uint64 i = 0; i < cmdline.size(); ++i)
			{
				Bool found = false;
				for (CLIArg& arg : args)
				{
					for (auto const& prefix : arg.prefixes) 
					{
						Bool prefix_found = cmdline[i] == ToWideString(prefix);
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
			Int argc;
			Wchar** argv = CommandLineToArgvW(cmd_line.c_str(), &argc);
			Parse(argc, argv);
		}

	private:
		std::vector<CLIArg> args;
		mutable std::unordered_map<std::string, Uint32> prefix_arg_index_map;
	};
}
