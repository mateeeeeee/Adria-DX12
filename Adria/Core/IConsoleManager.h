#pragma once
#include <string>
#include <type_traits>
#include "Utilities/Delegate.h"

namespace adria
{
	class IConsoleCommand;
	class IConsoleVariable;

	class IConsoleObject
	{
	public:
		IConsoleObject() = default;
		virtual ~IConsoleObject() = default;

		virtual char const* GetHelp() const = 0;
		virtual void SetHelp(char const*) = 0;

		virtual IConsoleVariable* AsVariable()
		{
			return nullptr;
		}
		virtual IConsoleVariable* AsCommand()
		{
			return nullptr;
		}
	};

	DECLARE_DELEGATE(ConsoleVariableDelegate, IConsoleVariable*)
	DECLARE_MULTICAST_DELEGATE(ConsoleVariableMulticastDelegate, IConsoleVariable*)
	class IConsoleVariable : public IConsoleObject
	{
	public:
		virtual void Set(char const* value) = 0;

		virtual bool IsBool() const { return false; }
		virtual bool IsInt() const { return false; }
		virtual bool IsFloat() const { return false; }
		virtual bool IsString() const { return false; }
		virtual bool IsVector() const { return false; }

		virtual int& GetIntRef() = 0;
		virtual float& GetFloatRef() = 0;
		virtual bool& GetBoolRef() = 0;
		virtual std::string& GetStringRef() = 0;
		virtual Vector3& GetVectorRef() = 0;

		virtual int GetInt() const = 0;
		virtual float GetFloat() const = 0;
		virtual bool GetBool() const = 0;
		virtual std::string GetString() const = 0;
		virtual Vector3 GetVector() const = 0;

		virtual void SetOnChangedCallback(ConsoleVariableDelegate const&) = 0;
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
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, Vector3 const& default_value, char const* help) = 0;

		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, bool& value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, int& value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, float& value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, std::string& value, char const* help) = 0;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, Vector3& value, char const* help) = 0;

		virtual IConsoleCommand* RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandDelegate const& command) = 0;
		virtual IConsoleCommand* RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandWithArgsDelegate const& command) = 0;

		virtual void UnregisterConsoleObject(IConsoleObject* obj) = 0;
		virtual void UnregisterConsoleObject(char const* name) = 0;

		virtual IConsoleVariable* FindConsoleVariable(char const* name) const = 0;
		virtual IConsoleCommand* FindConsoleCommand(char const* name) const = 0;
		virtual IConsoleObject* FindConsoleObject(char const* name) const = 0;

		virtual bool ProcessInput(char const* cmd) = 0;

	protected:
		virtual ~IConsoleManager() { }
	};
}