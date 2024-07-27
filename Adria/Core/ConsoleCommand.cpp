#include "ConsoleCommand.h"
#include "ConsoleManager.h"

namespace adria
{
	IConsoleCommand::IConsoleCommand(char const* name) : name(name)
	{
		g_ConsoleManager.RegisterConsoleCommand(this, name);
	}
}

