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
		virtual void ForAllObjects(ConsoleObjectDelegate const&) const override;

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

	class AutoConsoleVariable : public AutoConsoleObject
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
		AutoConsoleVariable(char const* name, std::string const& default_value, char const* help)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
		}

		AutoConsoleVariable(char const* name, bool default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariable(char const* name, int32 default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariable(char const* name, float default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariable(name, default_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariable(char const* name, std::string const& default_value, char const* help, ConsoleVariableDelegate const& callback)
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
		TAutoConsoleVariable(char const* name, std::type_identity_t<T> default_value, char const* help) : AutoConsoleVariable(name, default_value, help) {}
		TAutoConsoleVariable(char const* name, std::type_identity_t<T> default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleVariable(name, default_value, help, callback) {}

		T Get() const
		{
			if constexpr (std::is_same_v<T, bool>) return AsVariable()->GetBool();
			if constexpr (std::is_same_v<T, int>) return  AsVariable()->GetInt();
			if constexpr (std::is_same_v<T, float>) return AsVariable()->GetFloat();
			if constexpr (std::is_same_v<T, std::string>) return AsVariable()->GetString();
		}
		T* GetPtr()
		{
			if constexpr (std::is_same_v<T, bool>) return AsVariable()->GetBoolPtr();
			if constexpr (std::is_same_v<T, int>) return  AsVariable()->GetIntPtr();
			if constexpr (std::is_same_v<T, float>) return AsVariable()->GetFloatPtr();
			if constexpr (std::is_same_v<T, std::string>) return AsVariable()->GetStringPtr();
		}

	private:
	};

	template <uint32 N>
	TAutoConsoleVariable(char const* name, const char(&)[N], char const* help) -> TAutoConsoleVariable<std::string>;
	template <uint32 N>
	TAutoConsoleVariable(char const* name, const char(&)[N], char const* help, ConsoleVariableDelegate const& callback) -> TAutoConsoleVariable<std::string>;

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
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariableRef(char const* name, float& ref_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariableRef(char const* name, bool& ref_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleObject(g_ConsoleManager.RegisterConsoleVariableRef(name, ref_value, help))
		{
			AsVariable()->AddOnChanged(callback);
		}
		AutoConsoleVariableRef(char const* name, std::string& ref_value, char const* help, ConsoleVariableDelegate const& callback)
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
		TAutoConsoleVariableRef(char const* name, std::type_identity_t<T>& default_value, char const* help) : AutoConsoleVariableRef(name, default_value, help) {}
		TAutoConsoleVariableRef(char const* name, std::type_identity_t<T>& default_value, char const* help, ConsoleVariableDelegate const& callback)
			: AutoConsoleVariableRef(name, default_value, help, callback) {}

		T Get() const
		{
			if constexpr (std::is_same_v<T, bool>) return AsVariable()->GetBool();
			if constexpr (std::is_same_v<T, int>) return  AsVariable()->GetInt();
			if constexpr (std::is_same_v<T, float>) return AsVariable()->GetFloat();
			if constexpr (std::is_same_v<T, std::string>) return AsVariable()->GetString();
		}
		T* GetPtr() const
		{
			if constexpr (std::is_same_v<T, bool>) return AsVariable()->GetBoolPtr();
			if constexpr (std::is_same_v<T, int>) return  AsVariable()->GetIntPtr();
			if constexpr (std::is_same_v<T, float>) return AsVariable()->GetFloatPtr();
			if constexpr (std::is_same_v<T, std::string>) return AsVariable()->GetStringPtr();
		}

	private:
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