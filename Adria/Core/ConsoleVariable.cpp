#include "ConsoleVariable.h"
#include "ConsoleManager.h"

namespace adria
{
	IConsoleVariable::IConsoleVariable(char const* name) : name(name)
	{
		g_ConsoleManager.RegisterConsoleVariable(this, name);
	}
}
