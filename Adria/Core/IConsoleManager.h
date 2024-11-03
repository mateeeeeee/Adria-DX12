#pragma once
#include <string>
#include <type_traits>
#include "Utilities/Delegate.h"

namespace adria
{
	class IConsoleObject;
	class IConsoleCommand;
	class IConsoleVariable;

	DECLARE_DELEGATE(ConsoleObjectDelegate, IConsoleObject* const)
	class IConsoleObject
	{
	public:
		IConsoleObject() = default;
		virtual ~IConsoleObject() = default;

		virtual Char const* GetHelp() const = 0;
		virtual void SetHelp(Char const*) = 0;

		virtual Char const* GetName() const = 0;
		virtual void SetName(Char const*) = 0;

		virtual IConsoleVariable* AsVariable()
		{
			return nullptr;
		}
		virtual IConsoleCommand* AsCommand()
		{
			return nullptr;
		}
	};

	DECLARE_DELEGATE(ConsoleVariableDelegate, IConsoleVariable*)
	DECLARE_MULTICAST_DELEGATE(ConsoleVariableMulticastDelegate, IConsoleVariable*)
	class IConsoleVariable : public IConsoleObject
	{
	public:
		virtual Bool Set(Char const* value) = 0;
		virtual Bool Set(Bool value) = 0;
		virtual Bool Set(int value) = 0;
		virtual Bool Set(Float value) = 0;

		virtual Bool IsBool() const { return false; }
		virtual Bool IsInt() const { return false; }
		virtual Bool IsFloat() const { return false; }
		virtual Bool IsString() const { return false; }

		virtual int* GetIntPtr() { return nullptr; }
		virtual Float* GetFloatPtr() { return nullptr; }
		virtual Bool* GetBoolPtr() { return nullptr; }
		virtual std::string* GetStringPtr() { return nullptr; }

		virtual int GetInt() const = 0;
		virtual Float GetFloat() const = 0;
		virtual Bool GetBool() const = 0;
		virtual std::string GetString() const = 0;

		virtual void AddOnChanged(ConsoleVariableDelegate const&) = 0;
		virtual ConsoleVariableMulticastDelegate& OnChangedDelegate() = 0;
	};

	DECLARE_DELEGATE(ConsoleCommandDelegate)
	DECLARE_DELEGATE(ConsoleCommandWithArgsDelegate, std::span<Char const*>)
	class IConsoleCommand : public IConsoleObject
	{
	public:
		virtual Bool Execute(std::span<Char const*> args) = 0;
	};

	class IConsoleManager
	{
	public:

		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, Bool default_value, Char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, int default_value, Char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, Float default_value, Char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, Char const* default_value, Char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, std::string const& default_value, Char const* help) = 0;

		virtual IConsoleVariable* RegisterConsoleVariableRef(Char const* name, Bool& value, Char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(Char const* name, int& value, Char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(Char const* name, Float& value, Char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(Char const* name, std::string& value, Char const* help) = 0;

		virtual IConsoleCommand* RegisterConsoleCommand(Char const* name, Char const* help, ConsoleCommandDelegate const& command) = 0;
		virtual IConsoleCommand* RegisterConsoleCommand(Char const* name, Char const* help, ConsoleCommandWithArgsDelegate const& command) = 0;

		virtual void UnregisterConsoleObject(IConsoleObject* obj) = 0;
		virtual void UnregisterConsoleObject(std::string const& name) = 0;

		virtual IConsoleVariable* FindConsoleVariable(std::string const& name) const = 0;
		virtual IConsoleCommand* FindConsoleCommand(std::string const& name) const = 0;
		virtual IConsoleObject* FindConsoleObject(std::string const& name) const = 0;
		virtual void ForAllObjects(ConsoleObjectDelegate const&) const = 0;

		virtual Bool ProcessInput(std::string const& cmd) = 0;


	protected:
		virtual ~IConsoleManager() { }
	};
}