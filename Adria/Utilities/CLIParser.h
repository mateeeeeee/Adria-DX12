#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "StringUtil.h"

namespace adria
{
	class CLIParser;

	class CLIArg
	{
		friend class CLIParser;
	public:
		CLIArg() = default;
		CLIArg(std::vector<std::string>&& prefixes, Bool has_value) : prefixes(std::move(prefixes)), has_value(has_value) {}

		Bool AsBool(Bool default_value = false) const
		{
			ADRIA_ASSERT(has_value);
			if (values.empty()) return default_value;
			if (values[0] == "true" || values[0] == "1") return true;
			if (values[0] == "false" || values[0] == "0") return false;
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
			return (Int)strtol(values[0].c_str(), nullptr, 10);
		}
		Int AsIntOr(Int def) const
		{
			if (IsPresent()) return AsInt();
			else return def;
		}

		Float AsFloat() const
		{
			ADRIA_ASSERT(has_value);
			return (Float)std::strtod(values[0].c_str(), nullptr);
		}
		Float AsFloatOr(Float def) const
		{
			if (IsPresent()) return AsFloat();
			else return def;
		}

		std::string AsString() const
		{
			ADRIA_ASSERT(has_value);
			return values.empty() ? "" : values[0];
		}
		std::string AsStringOr(std::string const& def) const
		{
			if (IsPresent()) return AsString();
			else return def;
		}

		std::vector<std::string> AsStrings() const
		{
			ADRIA_ASSERT(has_value);
			return values;
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
		std::vector<std::string> values;
		Bool is_present = false;

		void AddValue(std::string const& value)
		{
			ADRIA_ASSERT(has_value);
			values.push_back(value);
		}
		void SetIsPresent()
		{
			is_present = true;
		}
	};

	class CLIParseResult
	{
		friend class CLIParser;
	public:

		CLIArg const& operator[](std::convertible_to<std::string> auto const& prefix) const
		{
			ADRIA_ASSERT_MSG(cli_arg_map.contains(prefix), "Did you forgot to add this Arg to CLIParser?");
			return cli_arg_map.find(prefix)->second;
		}

	private:
		std::unordered_map<std::string, CLIArg> cli_arg_map;

	private:
		CLIParseResult(std::vector<CLIArg> const& args, std::unordered_map<std::string, Uint32> const& prefix_index_map);
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
		ADRIA_NODISCARD CLIParseResult Parse(Int argc, Wchar** argv);
		ADRIA_NODISCARD CLIParseResult Parse(std::wstring const& cmd_line);

	private:
		std::vector<CLIArg> args;
		std::unordered_map<std::string, Uint32> prefix_arg_index_map;
	};
}
