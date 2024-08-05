#include "ConsoleManager.h"
#include "Utilities/StringUtil.h"

namespace adria
{
	class ConsoleVariableBase : public IConsoleVariable
	{
	public:
		ConsoleVariableBase(char const* name, char const* help)
		{
			SetName(name);
			SetHelp(help);
		}

		virtual char const* GetHelp() const override { return help.c_str(); }
		virtual void SetHelp(char const* _help) override { help = _help; }

		virtual char const* GetName() const override { return name.c_str(); }
		virtual void SetName(char const* _name) override { name = _name; }

		virtual void AddOnChanged(ConsoleVariableDelegate const& delegate)
		{
			on_changed_callback.Add(delegate);
		}
		virtual ConsoleVariableMulticastDelegate& OnChangedDelegate() override { return on_changed_callback; }

		virtual class IConsoleVariable* AsVariable() override
		{
			return this;
		}

	private:
		std::string name;
		std::string help;
		ConsoleVariableMulticastDelegate on_changed_callback;
	};

	namespace detail
	{
		template<typename T>
		class ConsoleVariableConversionHelper
		{
		public:
			static bool GetBool(T Value);
			static int32 GetInt(T Value);
			static float GetFloat(T Value);
			static std::string GetString(T Value);
		};
		template<> bool ConsoleVariableConversionHelper<bool>::GetBool(bool Value)
		{
			return Value;
		}
		template<> int32 ConsoleVariableConversionHelper<bool>::GetInt(bool Value)
		{
			return Value ? 1 : 0;
		}
		template<> float ConsoleVariableConversionHelper<bool>::GetFloat(bool Value)
		{
			return Value ? 1.0f : 0.0f;
		}
		template<> std::string ConsoleVariableConversionHelper<bool>::GetString(bool Value)
		{
			return Value ? "true" : "false";
		}
		template<> bool ConsoleVariableConversionHelper<int32>::GetBool(int32 Value)
		{
			return Value != 0;
		}
		template<> int32 ConsoleVariableConversionHelper<int32>::GetInt(int32 Value)
		{
			return Value;
		}
		template<> float ConsoleVariableConversionHelper<int32>::GetFloat(int32 Value)
		{
			return (float)Value;
		}
		template<> std::string ConsoleVariableConversionHelper<int32>::GetString(int32 Value)
		{
			return std::to_string(Value);
		}
		template<> bool ConsoleVariableConversionHelper<float>::GetBool(float Value)
		{
			return Value != 0.0f;
		}
		template<> int32 ConsoleVariableConversionHelper<float>::GetInt(float Value)
		{
			return (int32)Value;
		}
		template<> float ConsoleVariableConversionHelper<float>::GetFloat(float Value)
		{
			return Value;
		}
		template<> std::string ConsoleVariableConversionHelper<float>::GetString(float Value)
		{
			return std::to_string(Value);
		}
		template<> bool ConsoleVariableConversionHelper<std::string>::GetBool(std::string Value)
		{
			bool out = false;
			FromCString(Value.c_str(), out);
			return out;
		}
		template<> int32 ConsoleVariableConversionHelper<std::string>::GetInt(std::string Value)
		{
			int32 out = false;
			FromCString(Value.c_str(), out);
			return out;
		}
		template<> float ConsoleVariableConversionHelper<std::string>::GetFloat(std::string Value)
		{
			float out = 0.0f;
			FromCString(Value.c_str(), out);
			return out;
		}
		template<> std::string ConsoleVariableConversionHelper<std::string>::GetString(std::string Value)
		{
			return Value;
		}
	}

	class ConsoleCommandBase : public IConsoleCommand
	{
	public:
		ConsoleCommandBase(char const* name, char const* help)
		{
			SetName(name);
			SetHelp(help);
		}

		virtual char const* GetHelp() const override { return help.c_str(); }
		virtual void SetHelp(char const* _help) override { help = _help; }

		virtual char const* GetName() const override { return name.c_str(); }
		virtual void SetName(char const* _name) override { name = _name; }

		virtual IConsoleCommand* AsCommand() override
		{
			return this;
		}

	private: 
		std::string name;
		std::string help;
	};

	template <typename T>
	class ConsoleVariable : public ConsoleVariableBase
	{
	public:
		ConsoleVariable(T default_value, char const* name, char const* help) : ConsoleVariableBase(name, help), value(default_value)
		{
		}

		virtual bool Set(char const* str_value) override
		{
			T out;
			if (FromCString(str_value, out))
			{
				value = out;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual bool Set(bool bool_value) override
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				value = bool_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				value = detail::ConsoleVariableConversionHelper<bool>::GetInt(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				value = detail::ConsoleVariableConversionHelper<bool>::GetFloat(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<bool>::GetString(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual bool Set(int int_value) override
		{
			if constexpr (std::is_same_v<T, int>)
			{
				value = int_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				value = detail::ConsoleVariableConversionHelper<int>::GetInt(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				value = detail::ConsoleVariableConversionHelper<int>::GetFloat(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<int>::GetString(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual bool Set(float float_value) override
		{
			if constexpr (std::is_same_v<T, float>)
			{
				value = float_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				value = detail::ConsoleVariableConversionHelper<float>::GetInt(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				value = detail::ConsoleVariableConversionHelper<float>::GetBool(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<float>::GetString(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}

		virtual bool GetBool() const override { return detail::ConsoleVariableConversionHelper<T>::GetBool(value); }
		virtual int32 GetInt() const override { return detail::ConsoleVariableConversionHelper<T>::GetInt(value); }
		virtual float GetFloat() const override { return detail::ConsoleVariableConversionHelper<T>::GetFloat(value); }
		virtual std::string GetString() const override { return detail::ConsoleVariableConversionHelper<T>::GetString(value); }

		virtual bool IsBool() const override { return false; }
		virtual bool IsInt() const override { return false; }
		virtual bool IsFloat() const override { return false; }
		virtual bool IsString() const override { return false; }

		virtual bool* GetBoolPtr() override { return nullptr; }
		virtual int32* GetIntPtr() override { return nullptr; }
		virtual float* GetFloatPtr() override { return nullptr; }
		virtual std::string* GetStringPtr() override { return nullptr; }

	private:
		T value;
	};

	template<> bool ConsoleVariable<bool>::IsBool() const
	{
		return true;
	}
	template<> bool ConsoleVariable<int32>::IsInt() const
	{
		return true;
	}
	template<> bool ConsoleVariable<float>::IsFloat() const
	{
		return true;
	}
	template<> bool ConsoleVariable<std::string>::IsString() const
	{
		return true;
	}
	template<> bool* ConsoleVariable<bool>::GetBoolPtr()
	{
		return &value;
	}
	template<> int32* ConsoleVariable<int32>::GetIntPtr()
	{
		return &value;
	}
	template<> float* ConsoleVariable<float>::GetFloatPtr()
	{
		return &value;
	}
	template<> std::string* ConsoleVariable<std::string>::GetStringPtr()
	{
		return &value;
	}

	template <typename T>
	class ConsoleVariableRef : public ConsoleVariableBase
	{
	public:
		ConsoleVariableRef(T& ref_value, char const* name, char const* help) : ConsoleVariableBase(name, help), value(ref_value)
		{
		}

		virtual bool Set(char const* str_value) override
		{
			if (FromCString(str_value, value))
			{
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual bool Set(bool bool_value) override
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				value = bool_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				value = detail::ConsoleVariableConversionHelper<bool>::GetInt(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				value = detail::ConsoleVariableConversionHelper<bool>::GetFloat(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<bool>::GetString(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual bool Set(int int_value) override
		{
			if constexpr (std::is_same_v<T, int>)
			{
				value = int_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				value = detail::ConsoleVariableConversionHelper<int>::GetInt(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				value = detail::ConsoleVariableConversionHelper<int>::GetFloat(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<int>::GetString(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual bool Set(float float_value) override
		{
			if constexpr (std::is_same_v<T, float>)
			{
				value = float_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				value = detail::ConsoleVariableConversionHelper<float>::GetInt(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				value = detail::ConsoleVariableConversionHelper<float>::GetBool(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<float>::GetString(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}


		virtual bool GetBool() const override { return detail::ConsoleVariableConversionHelper<T>::GetBool(value); }
		virtual int32 GetInt() const override { return detail::ConsoleVariableConversionHelper<T>::GetInt(value); }
		virtual float GetFloat() const override { return detail::ConsoleVariableConversionHelper<T>::GetFloat(value); }
		virtual std::string GetString() const override { return detail::ConsoleVariableConversionHelper<T>::GetString(value); }

		virtual bool IsBool() const override { return false; }
		virtual bool IsInt() const override { return false; }
		virtual bool IsFloat() const override { return false; }
		virtual bool IsString() const override { return false; }

	private:
		T& value;
	};

	template<> bool ConsoleVariableRef<bool>::IsBool() const
	{
		return true;
	}
	template<> bool ConsoleVariableRef<int32>::IsInt() const
	{
		return true;
	}
	template<> bool ConsoleVariableRef<float>::IsFloat() const
	{
		return true;
	}
	template<> bool ConsoleVariableRef<std::string>::IsString() const
	{
		return true;
	}

	class ConsoleCommand : public ConsoleCommandBase
	{

	public:
		ConsoleCommand(ConsoleCommandDelegate const& delegate, char const* name, char const* help)
			: ConsoleCommandBase(name, help), delegate(delegate)
		{}

		virtual bool Execute(std::span<char const*> args) override
		{
			delegate.ExecuteIfBound();
			return true;
		}

	private:
		ConsoleCommandDelegate delegate;
	};

	class ConsoleCommandWithArgs : public ConsoleCommandBase
	{

	public:
		ConsoleCommandWithArgs(ConsoleCommandWithArgsDelegate const& delegate, char const* name, char const* help)
			: ConsoleCommandBase(name, help), delegate(delegate)
		{
		}

		virtual bool Execute(std::span<char const*> args) override
		{
			delegate.ExecuteIfBound(args);
			return true;
		}

	private:
		ConsoleCommandWithArgsDelegate delegate;
	};
	
	ConsoleManager::~ConsoleManager()
	{
		for (auto& [name, obj] : console_objects) delete obj;
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, bool default_value, char const* help)
	{
		return AddObject(name, new ConsoleVariable<bool>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, int default_value, char const* help)
	{
		return AddObject(name, new ConsoleVariable<int>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, float default_value, char const* help)
	{
		return AddObject(name, new ConsoleVariable<float>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, char const* default_value, char const* help)
	{
		return AddObject(name, new ConsoleVariable<std::string>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(char const* name, std::string const& default_value, char const* help)
	{
		return AddObject(name, new ConsoleVariable<std::string>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, bool& value, char const* help)
	{
		return AddObject(name, new ConsoleVariableRef<bool>(value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, int& value, char const* help)
	{
		return AddObject(name, new ConsoleVariableRef<int>(value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, float& value, char const* help)
	{
		return AddObject(name, new ConsoleVariableRef<float>(value, name, help))->AsVariable();

	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(char const* name, std::string& value, char const* help)
	{
		return AddObject(name, new ConsoleVariableRef<std::string>(value, name, help))->AsVariable();
	}

	IConsoleCommand* ConsoleManager::RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandDelegate const& command)
	{
		return AddObject(name, new ConsoleCommand(command, name, help))->AsCommand();
	}

	IConsoleCommand* ConsoleManager::RegisterConsoleCommand(char const* name, char const* help, ConsoleCommandWithArgsDelegate const& command)
	{
		return AddObject(name, new ConsoleCommandWithArgs(command, name, help))->AsCommand();
	}

	void ConsoleManager::UnregisterConsoleObject(IConsoleObject* console_obj)
	{
		for (auto& [name, obj] : console_objects)
		{
			if (console_obj == obj)
			{
				UnregisterConsoleObject(name);
				break;
			}
		}
	}

	void ConsoleManager::UnregisterConsoleObject(std::string const& name)
	{
		console_objects.erase(std::string(name));
	}

	IConsoleVariable* ConsoleManager::FindConsoleVariable(std::string const& name) const
	{
		IConsoleObject* object = FindConsoleObject(name);
		return object ? object->AsVariable() : nullptr;
	}

	IConsoleCommand* ConsoleManager::FindConsoleCommand(std::string const& name) const
	{
		IConsoleObject* object = FindConsoleObject(name);
		return object ? object->AsCommand() : nullptr;
	}

	IConsoleObject* ConsoleManager::FindConsoleObject(std::string const& name) const
	{
		if (!console_objects.contains(name)) return nullptr;
		return console_objects.find(name)->second;
	}

	void ConsoleManager::ForAllObjects(ConsoleObjectDelegate const& delegate) const
	{
		for (auto& [name, obj] : console_objects) delegate(obj);
	}

	bool ConsoleManager::ProcessInput(std::string const& cmd)
	{
		auto args = SplitString(cmd, ' ');
		if (args.empty()) return false;

		IConsoleObject* object = FindConsoleObject(args[0]);
		if (!object) return false;

		if (IConsoleVariable* cvar = object->AsVariable())
		{
			if (args.size() == 1) return false;
			return cvar->Set(args[1].c_str());
		}
		else if (IConsoleCommand* ccommand = object->AsCommand())
		{
			std::vector<char const*> command_args; command_args.reserve(args.size() - 1);
			for (uint64 i = 1; i < args.size(); ++i) command_args.push_back(args[i].c_str());
			return ccommand->Execute(command_args);
		}
		return false;
	}

	IConsoleObject* ConsoleManager::AddObject(char const* name, IConsoleObject* obj)
	{
		ADRIA_ASSERT(!console_objects.contains(name));
		console_objects[name] = obj;
		return obj;
	}

}

