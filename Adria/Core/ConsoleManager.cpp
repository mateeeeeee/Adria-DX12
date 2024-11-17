#include "ConsoleManager.h"
#include "Utilities/StringUtil.h"

namespace adria
{
	class ConsoleVariableBase : public IConsoleVariable
	{
	public:
		ConsoleVariableBase(Char const* name, Char const* help)
		{
			SetName(name);
			SetHelp(help);
		}

		virtual Char const* GetHelp() const override { return help.c_str(); }
		virtual void SetHelp(Char const* _help) override { help = _help; }

		virtual Char const* GetName() const override { return name.c_str(); }
		virtual void SetName(Char const* _name) override { name = _name; }

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
			static Bool GetBool(T Value);
			static Sint GetInt(T Value);
			static Float GetFloat(T Value);
			static std::string GetString(T Value);
		};
		template<> Bool ConsoleVariableConversionHelper<Bool>::GetBool(Bool Value)
		{
			return Value;
		}
		template<> Sint ConsoleVariableConversionHelper<Bool>::GetInt(Bool Value)
		{
			return Value ? 1 : 0;
		}
		template<> Float ConsoleVariableConversionHelper<Bool>::GetFloat(Bool Value)
		{
			return Value ? 1.0f : 0.0f;
		}
		template<> std::string ConsoleVariableConversionHelper<Bool>::GetString(Bool Value)
		{
			return Value ? "true" : "false";
		}
		template<> Bool ConsoleVariableConversionHelper<Sint>::GetBool(Sint Value)
		{
			return Value != 0;
		}
		template<> Sint ConsoleVariableConversionHelper<Sint>::GetInt(Sint Value)
		{
			return Value;
		}
		template<> Float ConsoleVariableConversionHelper<Sint>::GetFloat(Sint Value)
		{
			return (Float)Value;
		}
		template<> std::string ConsoleVariableConversionHelper<Sint>::GetString(Sint Value)
		{
			return std::to_string(Value);
		}
		template<> Bool ConsoleVariableConversionHelper<Float>::GetBool(Float Value)
		{
			return Value != 0.0f;
		}
		template<> Sint ConsoleVariableConversionHelper<Float>::GetInt(Float Value)
		{
			return (Sint)Value;
		}
		template<> Float ConsoleVariableConversionHelper<Float>::GetFloat(Float Value)
		{
			return Value;
		}
		template<> std::string ConsoleVariableConversionHelper<Float>::GetString(Float Value)
		{
			return std::to_string(Value);
		}
		template<> Bool ConsoleVariableConversionHelper<std::string>::GetBool(std::string Value)
		{
			Bool out = false;
			FromCString(Value.c_str(), out);
			return out;
		}
		template<> Sint ConsoleVariableConversionHelper<std::string>::GetInt(std::string Value)
		{
			Sint out = false;
			FromCString(Value.c_str(), out);
			return out;
		}
		template<> Float ConsoleVariableConversionHelper<std::string>::GetFloat(std::string Value)
		{
			Float out = 0.0f;
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
		ConsoleCommandBase(Char const* name, Char const* help)
		{
			SetName(name);
			SetHelp(help);
		}

		virtual Char const* GetHelp() const override { return help.c_str(); }
		virtual void SetHelp(Char const* _help) override { help = _help; }

		virtual Char const* GetName() const override { return name.c_str(); }
		virtual void SetName(Char const* _name) override { name = _name; }

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
		ConsoleVariable(T default_value, Char const* name, Char const* help) : ConsoleVariableBase(name, help), value(default_value)
		{
		}

		virtual Bool Set(Char const* str_value) override
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
		virtual Bool Set(Bool bool_value) override
		{
			if constexpr (std::is_same_v<T, Bool>)
			{
				value = bool_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Sint>)
			{
				value = detail::ConsoleVariableConversionHelper<Bool>::GetInt(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Float>)
			{
				value = detail::ConsoleVariableConversionHelper<Bool>::GetFloat(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<Bool>::GetString(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual Bool Set(Sint int_value) override
		{
			if constexpr (std::is_same_v<T, Sint>)
			{
				value = int_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Bool>)
			{
				value = detail::ConsoleVariableConversionHelper<Sint>::GetInt(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Float>)
			{
				value = detail::ConsoleVariableConversionHelper<Sint>::GetFloat(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<Sint>::GetString(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual Bool Set(Float float_value) override
		{
			if constexpr (std::is_same_v<T, Float>)
			{
				value = float_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Sint>)
			{
				value = detail::ConsoleVariableConversionHelper<Float>::GetInt(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Bool>)
			{
				value = detail::ConsoleVariableConversionHelper<Float>::GetBool(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<Float>::GetString(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}

		virtual Bool GetBool() const override { return detail::ConsoleVariableConversionHelper<T>::GetBool(value); }
		virtual Sint GetInt() const override { return detail::ConsoleVariableConversionHelper<T>::GetInt(value); }
		virtual Float GetFloat() const override { return detail::ConsoleVariableConversionHelper<T>::GetFloat(value); }
		virtual std::string GetString() const override { return detail::ConsoleVariableConversionHelper<T>::GetString(value); }

		virtual Bool IsBool() const override { return false; }
		virtual Bool IsInt() const override { return false; }
		virtual Bool IsFloat() const override { return false; }
		virtual Bool IsString() const override { return false; }

		virtual Bool* GetBoolPtr() override { return nullptr; }
		virtual Sint* GetIntPtr() override { return nullptr; }
		virtual Float* GetFloatPtr() override { return nullptr; }
		virtual std::string* GetStringPtr() override { return nullptr; }

	private:
		T value;
	};

	template<> Bool ConsoleVariable<Bool>::IsBool() const
	{
		return true;
	}
	template<> Bool ConsoleVariable<Sint>::IsInt() const
	{
		return true;
	}
	template<> Bool ConsoleVariable<Float>::IsFloat() const
	{
		return true;
	}
	template<> Bool ConsoleVariable<std::string>::IsString() const
	{
		return true;
	}
	template<> Bool* ConsoleVariable<Bool>::GetBoolPtr()
	{
		return &value;
	}
	template<> Sint* ConsoleVariable<Sint>::GetIntPtr()
	{
		return &value;
	}
	template<> Float* ConsoleVariable<Float>::GetFloatPtr()
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
		ConsoleVariableRef(T& ref_value, Char const* name, Char const* help) : ConsoleVariableBase(name, help), value(ref_value)
		{
		}

		virtual Bool Set(Char const* str_value) override
		{
			if (FromCString(str_value, value))
			{
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual Bool Set(Bool bool_value) override
		{
			if constexpr (std::is_same_v<T, Bool>)
			{
				value = bool_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Sint>)
			{
				value = detail::ConsoleVariableConversionHelper<Bool>::GetInt(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Float>)
			{
				value = detail::ConsoleVariableConversionHelper<Bool>::GetFloat(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<Bool>::GetString(bool_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}
		virtual Bool Set(Sint int_value) override
		{
			if constexpr (std::is_same_v<T, Sint>)
			{
				value = int_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Bool>)
			{
				value = detail::ConsoleVariableConversionHelper<Sint>::GetInt(int_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Float>)
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
		virtual Bool Set(Float float_value) override
		{
			if constexpr (std::is_same_v<T, Float>)
			{
				value = float_value;
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Sint>)
			{
				value = detail::ConsoleVariableConversionHelper<Float>::GetInt(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, Bool>)
			{
				value = detail::ConsoleVariableConversionHelper<Float>::GetBool(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = detail::ConsoleVariableConversionHelper<Float>::GetString(float_value);
				OnChangedDelegate().Broadcast(this);
				return true;
			}
			return false;
		}


		virtual Bool GetBool() const override { return detail::ConsoleVariableConversionHelper<T>::GetBool(value); }
		virtual Sint GetInt() const override { return detail::ConsoleVariableConversionHelper<T>::GetInt(value); }
		virtual Float GetFloat() const override { return detail::ConsoleVariableConversionHelper<T>::GetFloat(value); }
		virtual std::string GetString() const override { return detail::ConsoleVariableConversionHelper<T>::GetString(value); }

		virtual Bool IsBool() const override { return false; }
		virtual Bool IsInt() const override { return false; }
		virtual Bool IsFloat() const override { return false; }
		virtual Bool IsString() const override { return false; }

	private:
		T& value;
	};

	template<> Bool ConsoleVariableRef<Bool>::IsBool() const
	{
		return true;
	}
	template<> Bool ConsoleVariableRef<Sint>::IsInt() const
	{
		return true;
	}
	template<> Bool ConsoleVariableRef<Float>::IsFloat() const
	{
		return true;
	}
	template<> Bool ConsoleVariableRef<std::string>::IsString() const
	{
		return true;
	}

	class ConsoleCommand : public ConsoleCommandBase
	{

	public:
		ConsoleCommand(ConsoleCommandDelegate const& delegate, Char const* name, Char const* help)
			: ConsoleCommandBase(name, help), delegate(delegate)
		{}

		virtual Bool Execute(std::span<Char const*> args) override
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
		ConsoleCommandWithArgs(ConsoleCommandWithArgsDelegate const& delegate, Char const* name, Char const* help)
			: ConsoleCommandBase(name, help), delegate(delegate)
		{
		}

		virtual Bool Execute(std::span<Char const*> args) override
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

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(Char const* name, Bool default_value, Char const* help)
	{
		return AddObject(name, new ConsoleVariable<Bool>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(Char const* name, Sint default_value, Char const* help)
	{
		return AddObject(name, new ConsoleVariable<Sint>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(Char const* name, Float default_value, Char const* help)
	{
		return AddObject(name, new ConsoleVariable<Float>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(Char const* name, Char const* default_value, Char const* help)
	{
		return AddObject(name, new ConsoleVariable<std::string>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariable(Char const* name, std::string const& default_value, Char const* help)
	{
		return AddObject(name, new ConsoleVariable<std::string>(default_value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(Char const* name, Bool& value, Char const* help)
	{
		return AddObject(name, new ConsoleVariableRef<Bool>(value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(Char const* name, Sint& value, Char const* help)
	{
		return AddObject(name, new ConsoleVariableRef<Sint>(value, name, help))->AsVariable();
	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(Char const* name, Float& value, Char const* help)
	{
		return AddObject(name, new ConsoleVariableRef<Float>(value, name, help))->AsVariable();

	}

	IConsoleVariable* ConsoleManager::RegisterConsoleVariableRef(Char const* name, std::string& value, Char const* help)
	{
		return AddObject(name, new ConsoleVariableRef<std::string>(value, name, help))->AsVariable();
	}

	IConsoleCommand* ConsoleManager::RegisterConsoleCommand(Char const* name, Char const* help, ConsoleCommandDelegate const& command)
	{
		return AddObject(name, new ConsoleCommand(command, name, help))->AsCommand();
	}

	IConsoleCommand* ConsoleManager::RegisterConsoleCommand(Char const* name, Char const* help, ConsoleCommandWithArgsDelegate const& command)
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

	Bool ConsoleManager::ProcessInput(std::string const& cmd)
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
			std::vector<Char const*> command_args; command_args.reserve(args.size() - 1);
			for (Uint64 i = 1; i < args.size(); ++i) command_args.push_back(args[i].c_str());
			return ccommand->Execute(command_args);
		}
		return false;
	}

	IConsoleObject* ConsoleManager::AddObject(Char const* name, IConsoleObject* obj)
	{
		ADRIA_ASSERT(!console_objects.contains(name));
		console_objects[name] = obj;
		return obj;
	}

}

