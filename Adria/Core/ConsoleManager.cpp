#include "ConsoleManager.h"
#include "ConsoleVariable.h"
#include "ConsoleCommand.h"
#include "Utilities/StringUtil.h"

namespace adria
{
	void ConsoleManager::RegisterConsoleVariable(IConsoleVariable* cvar, char const* name)
	{
		std::string lower_name = ToLower(name);
		if (cvars.find(lower_name) == cvars.end())
		{
			cvars[lower_name] = cvar;
		}
	}

	void ConsoleManager::RegisterConsoleCommand(IConsoleCommand* ccmd, char const* name)
	{
		std::string lower_name = ToLower(name);
		if (ccmds.find(lower_name) == ccmds.end())
		{
			ccmds[lower_name] = ccmd;
		}
	}

	bool ConsoleManager::Execute(char const* cmd)
	{
		auto args = SplitString(cmd, ' ');
		if (args.empty()) return false;

		std::vector<char const*> c_args(std::max<uint64>(args.size(), 8));
		for (uint64 i = 0; i < args.size(); ++i) c_args[i] = args[i].c_str();

		if (auto it = cvars.find(args[0]); it != cvars.end())
		{
			auto& [name, cvar] = *it;
			if (args.size() == 1) return false;
			else return cvar->SetValue(args[1].c_str());
		}
		if (auto it = ccmds.find(args[0]); it != ccmds.end())
		{
			auto& [name, ccmd] = *it;
			return ccmd->Execute(std::span<char const*>(c_args.data() + 1, args.size() - 1));
		}
		return false;
	}
}

