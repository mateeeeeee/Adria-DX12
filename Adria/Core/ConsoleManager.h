#pragma once
#include <type_traits>
#include "../Utilities/HashMap.h"

namespace adria
{
	class IConsoleCommand;
	class IConsoleVariable;

	template<typename F>
	concept CVarCallback = requires(F f, IConsoleVariable * cvar)
	{
		{ f(cvar) } -> std::same_as<void>;
	};

	template<typename F>
	concept CCmdCallback = requires(F f, IConsoleCommand * ccmd)
	{
		{ f(ccmd) } -> std::same_as<void>;
	};

	class ConsoleManager
	{
		friend class IConsoleVariable;
		friend class IConsoleCommand;
	public:
		static bool Execute(char const* cmd);

		template<CVarCallback F>
		static void ForEachCVar(F&& pfn)
		{
			for (auto&& [name, cvar] : cvars) pfn(cvar);
		}
		template<CCmdCallback F>
		static void ForEachCCmd(F&& pfn)
		{
			for (auto&& [name, ccmd] : ccmds) pfn(ccmd);
		}
	private:
		inline static HashMap<std::string, IConsoleVariable*> cvars{};
		inline static HashMap<std::string, IConsoleCommand*>  ccmds{};

	private:
		static void RegisterConsoleVariable(IConsoleVariable* cvar, char const* name);
		static void RegisterConsoleCommand(IConsoleCommand* ccmd, char const* name);
	};

}