#pragma once
#include <string>
#include <type_traits>
#include "Utilities/Singleton.h"

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

	class ConsoleManager : public Singleton<ConsoleManager>
	{
		friend class Singleton<ConsoleManager>;
		friend class IConsoleVariable;
		friend class IConsoleCommand;
	public:
		bool Execute(char const* cmd);

		template<CVarCallback F>
		void ForEachCVar(F&& pfn)
		{
			for (auto&& [name, cvar] : cvars) pfn(cvar);
		}
		template<CCmdCallback F>
		void ForEachCCmd(F&& pfn)
		{
			for (auto&& [name, ccmd] : ccmds) pfn(ccmd);
		}
	private:
		std::unordered_map<std::string, IConsoleVariable*> cvars{};
		std::unordered_map<std::string, IConsoleCommand*>  ccmds{};

	private:
		void RegisterConsoleVariable(IConsoleVariable* cvar, char const* name);
		void RegisterConsoleCommand(IConsoleCommand* ccmd, char const* name);
	};
	#define g_ConsoleManager ConsoleManager::Get()

}