#pragma once
#include <span>
#include <functional>
#include <type_traits>
#include <tuple>
#include "Utilities/StringUtil.h"

namespace adria
{

	class IConsoleCommand
	{
	public:
		IConsoleCommand(char const* name);
		virtual ~IConsoleCommand() = default;
		virtual bool Execute(std::span<char const*> args) = 0;

		char const* const GetName() const { return name; }
	private:
		char const* name;
	};

	template<typename ...Args>
	class ConsoleCommand : public IConsoleCommand
	{
		template<uint64 I, typename... Args>
		void TupleFromArguments(std::tuple<Args...>& t, std::span<char const*> args)
		{
			if (!FromCString(args[I], std::get<I>(t))) return;
			if constexpr (I < sizeof...(Args) - 1)
			{
				TupleFromArguments<I + 1>(t, args);
			}
		}
		template<typename... Args>
		std::tuple<Args...> TupleFromArguments(std::span<char const*> args)
		{
			std::tuple<Args...> tuple;
			if constexpr (sizeof...(Args) > 0)
			{
				TupleFromArguments<0>(tuple, args);
			}
			return tuple;
		}

	public:
		ConsoleCommand(char const* name, std::function<void(Args...)>&& cmd_fn)
			: IConsoleCommand(name), fn(std::move(cmd_fn)) {}

		virtual bool Execute(std::span<char const*> args) override
		{
			if (args.size() == sizeof...(Args)) return ExecuteImpl(args, std::index_sequence_for<Args...>());
			else return false;
		}

	private:
		template<uint64... Is>
		bool ExecuteImpl(std::span<char const*> args, std::index_sequence<Is...>)
		{
			std::tuple<std::remove_reference_t<Args>...> arguments = TupleFromArguments<std::remove_reference_t<Args>...>(args);
			fn(std::get<Is>(arguments)...);
			return true;
		}
	private:
		std::function<void(Args...)> fn;
	};
}