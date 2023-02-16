#include "CVar.h"
#include "../Utilities/HashMap.h"

namespace adria
{
	namespace
	{
		HashMap<std::string, IConsoleVariable*> cvars;
	}

	void ConsoleManager::Initialize()
	{
		//todo 
	}

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
		//todo
		return true;
	}

}

