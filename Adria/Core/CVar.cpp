#include "CVar.h"
#include "../Utilities/StringUtil.h"

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

	bool ConsoleManager::Execute(char const* cmd)
	{
		auto args = SplitString(cmd, ' ');
		if (args.empty()) return false;

		if (auto it = cvars.find(args[0]); it != cvars.end())
		{
			auto& [name, cvar] = *it;
			if (args.size() == 1) return false;
			return cvar->SetValue(args[1].c_str());
		}
		return false;
	}
}

