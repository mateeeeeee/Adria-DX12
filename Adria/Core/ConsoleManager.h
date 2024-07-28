#pragma once
#include "IConsoleManager.h"
#include "Utilities/Singleton.h"

namespace adria
{

	class ConsoleManager : public IConsoleManager, public Singleton<ConsoleManager>
	{
		friend class Singleton<ConsoleManager>;
	public:
		ConsoleManager() {}
		~ConsoleManager();

		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, bool default_value, char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, int default_value, char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, float default_value, char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, char const* default_value, char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariable(char const* name, std::string const& default_value, char const* help) override;

		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, bool& value, char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, int& value, char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, float& value, char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariableRef(char const* name, std::string& value, char const* help) override;

		virtual IConsoleCommand* RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandDelegate const& command) override;
		virtual IConsoleCommand* RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandWithArgsDelegate const& command) override;

		virtual void UnregisterConsoleObject(IConsoleObject* obj) override;
		virtual void UnregisterConsoleObject(std::string const& name) override;

		virtual IConsoleVariable* FindConsoleVariable(std::string const& name) const override;
		virtual IConsoleCommand* FindConsoleCommand(std::string const& name) const override;
		virtual IConsoleObject* FindConsoleObject(std::string const& name) const override;

		virtual bool ProcessInput(std::string const& cmd) override;

	private:
		std::unordered_map<std::string, IConsoleObject*> console_objects;

	private:
		IConsoleObject* AddObject(char const* name, IConsoleObject* obj);
	};
	#define g_ConsoleManager ConsoleManager::Get()

	class AutoConsoleObject
	{

	public:
		virtual ~AutoConsoleObject()
		{
			g_ConsoleManager.UnregisterConsoleObject(target);
		}
		ADRIA_FORCEINLINE IConsoleVariable* AsVariable()
		{
			ADRIA_ASSERT(target->AsVariable());
			return static_cast<IConsoleVariable*>(target);
		}
		ADRIA_FORCEINLINE IConsoleVariable const* AsVariable() const
		{
			ADRIA_ASSERT(target->AsVariable());
			return static_cast<IConsoleVariable const*>(target);
		}

	protected:
		AutoConsoleObject(IConsoleObject* target) : target(target)
		{
			ADRIA_ASSERT(target);
		}

	private:
		IConsoleObject* target;
	};

	class AutoConsoleVariable : private AutoConsoleObject
	{
	public:
		AutoConsoleVariable(char const* name, bool default_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}
		AutoConsoleVariable(char const* name, int32 default_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}
		AutoConsoleVariable(char const* name, float default_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}
		AutoConsoleVariable(char const* name, char const* default_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}

		AutoConsoleVariable(char const* name, bool default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->SetOnChangedCallback(callback);
		}
		AutoConsoleVariable(char const* name, int32 default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->SetOnChangedCallback(callback);
		}
		AutoConsoleVariable(char const* name, float default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->SetOnChangedCallback(callback);
		}
		AutoConsoleVariable(char const* name, char const* default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->SetOnChangedCallback(callback);
		}

		ADRIA_FORCEINLINE IConsoleVariable& operator*()
		{
			return *AsVariable();
		}
		ADRIA_FORCEINLINE const IConsoleVariable& operator*() const
		{
			return *AsVariable();
		}
		ADRIA_FORCEINLINE IConsoleVariable* operator->()
		{
			return AsVariable();
		}
		ADRIA_FORCEINLINE const IConsoleVariable* operator->() const
		{
			return AsVariable();
		}
	};

	class AutoConsoleVariableRef : private AutoConsoleObject
	{
	public:
		AutoConsoleVariableRef(char const* name, int32& ref_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
		}
		AutoConsoleVariableRef(char const* name, float& ref_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
		}
		AutoConsoleVariableRef(char const* name, bool& ref_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
		}
		AutoConsoleVariableRef(char const* name, std::string& ref_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
		}

		AutoConsoleVariableRef(char const* name, int32& ref_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->SetOnChangedCallback(callback);
		}
		AutoConsoleVariableRef(char const* name, float& ref_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->SetOnChangedCallback(callback);
		}
		AutoConsoleVariableRef(char const* name, bool& ref_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->SetOnChangedCallback(callback);
		}
		AutoConsoleVariableRef(char const* name, std::string& ref_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->SetOnChangedCallback(callback);
		}

		ADRIA_FORCEINLINE IConsoleVariable& operator*()
		{
			return *AsVariable();
		}
		ADRIA_FORCEINLINE const IConsoleVariable& operator*() const
		{
			return *AsVariable();
		}
		ADRIA_FORCEINLINE IConsoleVariable* operator->()
		{
			return AsVariable();
		}
		ADRIA_FORCEINLINE const IConsoleVariable* operator->() const
		{
			return AsVariable();
		}
	};

	class AutoConsoleCommand : private AutoConsoleObject
	{
	public:
		AutoConsoleCommand(char const* name, char const* help, ConsoleCommandDelegate const& command)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleCommand(name, help, command))
		{
		}
		AutoConsoleCommand(char const* name, char const* help, ConsoleCommandWithArgsDelegate const& command)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleCommand(name, help, command))
		{
		}
	};

}