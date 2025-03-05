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

		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, Bool default_value, Char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, Int default_value, Char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, Float default_value, Char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, Char const* default_value, Char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariable(Char const* name, std::string const& default_value, Char const* help) override;

		virtual IConsoleVariable* RegisterConsoleVariableRef(Char const* name, Bool& value, Char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariableRef(Char const* name, Int& value, Char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariableRef(Char const* name, Float& value, Char const* help) override;
		virtual IConsoleVariable* RegisterConsoleVariableRef(Char const* name, std::string& value, Char const* help) override;

		virtual IConsoleCommand* RegisterConsoleCommand(Char const* name, Char const* help, ConsoleCommandDelegate const& command) override;
		virtual IConsoleCommand* RegisterConsoleCommand(Char const* name, Char const* help, ConsoleCommandWithArgsDelegate const& command) override;

		virtual void UnregisterConsoleObject(IConsoleObject* obj) override;
		virtual void UnregisterConsoleObject(std::string const& name) override;

		virtual IConsoleVariable* FindConsoleVariable(std::string const& name) const override;
		virtual IConsoleCommand* FindConsoleCommand(std::string const& name) const override;
		virtual IConsoleObject* FindConsoleObject(std::string const& name) const override;
		virtual void ForAllObjects(ConsoleObjectDelegate const&) const override;

		virtual Bool ProcessInput(std::string const& cmd) override;

	private:
		std::unordered_map<std::string, IConsoleObject*> console_objects;

	private:
		IConsoleObject* AddObject(Char const* name, IConsoleObject* obj);
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

	class AutoConsoleVariable : public AutoConsoleObject
	{
	public:
		AutoConsoleVariable(Char const* name, Bool default_value, Char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}
		AutoConsoleVariable(Char const* name, Int default_value, Char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}
		AutoConsoleVariable(Char const* name, Float default_value, Char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}
		AutoConsoleVariable(Char const* name, std::string const& default_value, Char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}

		AutoConsoleVariable(Char const* name, Bool default_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariable(Char const* name, Int default_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariable(Char const* name, Float default_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariable(Char const* name, std::string const& default_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->AddOnChanged(callback);
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

	template<typename T>
	class TAutoConsoleVariable final : public AutoConsoleVariable
	{
	public:
		TAutoConsoleVariable(Char const* name, std::type_identity_t<T> default_value, Char const* help) : AutoConsoleVariable(name, default_value, help) {}
		TAutoConsoleVariable(Char const* name, std::type_identity_t<T> default_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleVariable(name, default_value, help, callback) {}

		T Get() const
		{
			if constexpr (std::is_same_v<T, Bool>) return AsVariable()->GetBool();
			if constexpr (std::is_same_v<T, Int>) return  AsVariable()->GetInt();
			if constexpr (std::is_same_v<T, Float>) return AsVariable()->GetFloat();
			if constexpr (std::is_same_v<T, std::string>) return AsVariable()->GetString();
		}
		T* GetPtr()
		{
			if constexpr (std::is_same_v<T, Bool>) return AsVariable()->GetBoolPtr();
			if constexpr (std::is_same_v<T, Int>) return  AsVariable()->GetIntPtr();
			if constexpr (std::is_same_v<T, Float>) return AsVariable()->GetFloatPtr();
			if constexpr (std::is_same_v<T, std::string>) return AsVariable()->GetStringPtr();
		}

	private:
	};

	template <Uint32 N>
	TAutoConsoleVariable(Char const* name, const Char(&)[N], Char const* help) -> TAutoConsoleVariable<std::string>;
	template <Uint32 N>
	TAutoConsoleVariable(Char const* name, const Char(&)[N], Char const* help, ConsoleVariableDelegate const& callback) -> TAutoConsoleVariable<std::string>;

	class AutoConsoleVariableRef : private AutoConsoleObject
	{
	public:
		AutoConsoleVariableRef(Char const* name, Int& ref_value, Char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
		}
		AutoConsoleVariableRef(Char const* name, Float& ref_value, Char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
		}
		AutoConsoleVariableRef(Char const* name, Bool& ref_value, Char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
		}
		AutoConsoleVariableRef(Char const* name, std::string& ref_value, Char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
		}

		AutoConsoleVariableRef(Char const* name, Int& ref_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariableRef(Char const* name, Float& ref_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariableRef(Char const* name, Bool& ref_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariableRef(Char const* name, std::string& ref_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->AddOnChanged(callback);
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

	template<typename T>
	class TAutoConsoleVariableRef final : public AutoConsoleVariableRef
	{
	public:
		TAutoConsoleVariableRef(Char const* name, std::type_identity_t<T>& default_value, Char const* help) : AutoConsoleVariableRef(name, default_value, help) {}
		TAutoConsoleVariableRef(Char const* name, std::type_identity_t<T>& default_value, Char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleVariableRef(name, default_value, help, callback) {}

		T Get() const
		{
			if constexpr (std::is_same_v<T, Bool>) return AsVariable()->GetBool();
			if constexpr (std::is_same_v<T, Int>) return  AsVariable()->GetInt();
			if constexpr (std::is_same_v<T, Float>) return AsVariable()->GetFloat();
			if constexpr (std::is_same_v<T, std::string>) return AsVariable()->GetString();
		}
		T* GetPtr() const
		{
			if constexpr (std::is_same_v<T, Bool>) return AsVariable()->GetBoolPtr();
			if constexpr (std::is_same_v<T, Int>) return  AsVariable()->GetIntPtr();
			if constexpr (std::is_same_v<T, Float>) return AsVariable()->GetFloatPtr();
			if constexpr (std::is_same_v<T, std::string>) return AsVariable()->GetStringPtr();
		}

	private:
	};


	class AutoConsoleCommand : private AutoConsoleObject
	{
	public:
		AutoConsoleCommand(Char const* name, Char const* help, ConsoleCommandDelegate const& command)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleCommand(name, help, command))
		{
		}
		AutoConsoleCommand(Char const* name, Char const* help, ConsoleCommandWithArgsDelegate const& command)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleCommand(name, help, command))
		{
		}
	};

}