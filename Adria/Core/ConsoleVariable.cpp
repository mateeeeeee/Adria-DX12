#include "ConsoleVariable.h"
#include "ConsoleManager.h"

namespace adria
{
	IConsoleVariable::IConsoleVariable(char const* name) : name(name)
	{
		ConsoleManager::RegisterConsoleVariable(this, name);
	}

}

