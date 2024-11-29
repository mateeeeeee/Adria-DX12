#include <shellapi.h>
#include "CLIParser.h"

namespace adria
{
	CLIParseResult::CLIParseResult(std::vector<CLIArg> const& args, std::unordered_map<std::string, Uint32> const& prefix_index_map)
	{
		for (auto const& [prefix, index] : prefix_index_map)
		{
			cli_arg_map[prefix] = args[index];
		}
	}

	CLIParseResult CLIParser::Parse(std::wstring const& cmd_line)
	{
		Int argc;
		Wchar** argv = CommandLineToArgvW(cmd_line.c_str(), &argc);
		return Parse(argc, argv);
	}

	CLIParseResult CLIParser::Parse(Int argc, Wchar** argv)
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
		CLIParseResult result(args, prefix_arg_index_map);
		args.clear();
		prefix_arg_index_map.clear();
		return result;
	}

}

