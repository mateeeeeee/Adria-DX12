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

		virtual char const* GetHelp() const = 0;
		virtual void SetHelp(char const*) = 0;

		virtual char const* GetName() const = 0;
		virtual void SetName(char const*) = 0;

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
		virtual bool Set(char const* value) = 0;
		virtual bool Set(bool value) = 0;
		virtual bool Set(int value) = 0;
		virtual bool Set(float value) = 0;

		virtual bool IsBool() const { return false; }
		virtual bool IsInt() const { return false; }
		virtual bool IsFloat() const { return false; }
		virtual bool IsString() const { return false; }

		virtual int* GetIntPtr() { return nullptr; }
		virtual float* GetFloatPtr() { return nullptr; }
		virtual bool* GetBoolPtr() { return nullptr; }
		virtual std::string* GetStringPtr() { return nullptr; }

		virtual int GetInt() const = 0;
		virtual float GetFloat() const = 0;
		virtual bool GetBool() const = 0;
		virtual std::string GetString() const = 0;

		virtual void AddOnChanged(ConsoleVariableDelegate const&) = 0;
		virtual ConsoleVariableMulticastDelegate& OnChangedDelegate() = 0;
	};

	DECLARE_DELEGATE(ConsoleCommandDelegate)
	DECLARE_DELEGATE(ConsoleCommandWithArgsDelegate, std::span<char const*>)
	class IConsoleCommand : public IConsoleObject
	{
	public:
		virtual bool Execute(std::span<char const*> args) = 0;
	};

	class IConsoleManager
	{
	public:

		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, bool default_value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, int default_value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, float default_value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, char const* default_value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, std::string const& default_value, char const* help) = 0;

		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, bool& value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, int& value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, float& value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, std::string& value, char const* help) = 0;

		virtual IConsoleCommand* RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandDelegate const& command) = 0;
		virtual IConsoleCommand* RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandWithArgsDelegate const& command) = 0;

		virtual void UnregisterConsoleObject(IConsoleObject* obj) = 0;
		virtual void UnregisterConsoleObject(std::string const& name) = 0;

		virtual IConsoleVariable* FindConsoleVariable(std::string const& name) const = 0;
		virtual IConsoleCommand* FindConsoleCommand(std::string const& name) const = 0;
		virtual IConsoleObject* FindConsoleObject(std::string const& name) const = 0;
		virtual void ForAllObjects(ConsoleObjectDelegate const&) const = 0;

		virtual bool ProcessInput(std::string const& cmd) = 0;


	protected:
		virtual ~IConsoleManager() { }
	};
}