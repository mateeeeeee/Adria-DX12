#include "ConsoleManager.h"

namespace adria
{

	class ConsoleVariableBase : public IConsoleVariable
	{
	public:

		virtual char const* GetHelp() const override { return help.c_str(); }
		virtual void SetHelp(char const* _help) override { help = _help; }

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

		virtual void SetOnChangedCallback(ConsoleVariableDelegate const& delegate)
		{
			on_changed_callback.Add(delegate);
		}
		virtual ConsoleVariableMulticastDelegate& OnChangedDelegate() { return on_changed_callback; }

	private:
		std::string help;
		ConsoleVariableMulticastDelegate on_changed_callback;
	};
	
	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, bool default_value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, int default_value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, float default_value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, char const* default_value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, std::string const& default_value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, Vector3 const& default_value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, bool& value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, int& value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, float& value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, std::string& value, char const* help)
	{
		return nullptr;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, Vector3& value, char const* help)
	{
		return nullptr;
	}

	IConsoleCommand* ConsoleManager::RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandDelegate const& command)
	{
		return nullptr;
	}

	IConsoleCommand* ConsoleManager::RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandWithArgsDelegate const& command)
	{
		return nullptr;
	}

	void ConsoleManager::UnregisterConsoleObject(IConsoleObject* obj)
	{

	}

	void ConsoleManager::UnregisterConsoleObject(char const* name)
	{

	}

	IConsoleVariable* ConsoleManager::FindConsoleVariable(char const* name) const
	{
		return nullptr;
	}

	IConsoleCommand* ConsoleManager::FindConsoleCommand(char const* name) const
	{
		return nullptr;
	}

	IConsoleObject* ConsoleManager::FindConsoleObject(char const* name) const
	{
		return nullptr;
	}

	bool ConsoleManager::ProcessInput(char const* cmd)
	{
		return false;
	}

}

