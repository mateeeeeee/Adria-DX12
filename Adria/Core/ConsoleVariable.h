#pragma once
#include <string>
#include "../Utilities/StringUtil.h"

namespace adria
{
	class IConsoleVariable
	{
	public:
		IConsoleVariable(char const* name);
		virtual ~IConsoleVariable() = default;

		[[maybe_unused]] virtual bool SetValue(const char* pValue) = 0;

		char const* const GetName() const { return name; }

		[[nodiscard]] virtual int AsInt() const = 0;
		[[nodiscard]] virtual float AsFloat() const = 0;
		[[nodiscard]] virtual bool AsBool() const = 0;
		[[nodiscard]] virtual std::string AsString() const = 0;

	private:
		char const* name;
	};

	template<typename CVarType>
	class ConsoleVariable : public IConsoleVariable
	{
	public:
		ConsoleVariable(char const* name, CVarType&& def)
			: IConsoleVariable(name), value(std::forward<CVarType>(def))
		{}

		void SetValue(CVarType const& v)
		{
			value = v;
		}
		[[maybe_unused]] virtual bool SetValue(const char* str_value) override
		{
			CVarType value;
			if (FromCString(str_value, value))
			{
				SetValue(value);
				return true;
			}
			return false;
		}

		[[nodiscard]] virtual int AsInt() const override
		{
			if constexpr (std::is_same_v<CVarType, int>)
			{
				return value;
			}
			else if constexpr (std::is_same_v<CVarType, bool>)
			{
				return value ? 1 : 0;
			}
			else if constexpr (std::is_same_v<CVarType, float>)
			{
				return static_cast<int>(value);
			}
			else if constexpr (std::is_same_v<CVarType, const char*>)
			{
				int out;
				ADRIA_ASSERT(FromCString(value, out));
				return out;
			}
		}
		[[nodiscard]] virtual float AsFloat() const override
		{
			if constexpr (std::is_same_v<CVarType, int>)
			{
				return static_cast<float>(value);
			}
			else if constexpr (std::is_same_v<CVarType, bool>)
			{
				return value ? 1.0f : 0.0f;
			}
			else if constexpr (std::is_same_v<CVarType, float>)
			{
				return value;
			}
			else if constexpr (std::is_same_v<CVarType, const char*>)
			{
				float out;
				ADRIA_ASSERT(FromCString(value, out));
				return out;
			}
		}
		[[nodiscard]] virtual bool AsBool() const override
		{
			if constexpr (std::is_same_v<CVarType, int>)
			{
				return value > 0;
			}
			else if constexpr (std::is_same_v<CVarType, bool>)
			{
				return value;
			}
			else if constexpr (std::is_same_v<CVarType, float>)
			{
				return value > 0.0f;
			}
			else if constexpr (std::is_same_v<CVarType, const char*>)
			{
				bool out;
				ADRIA_ASSERT(FromCString(value, out));
				return out;
			}
		}
		[[nodiscard]] virtual std::string AsString() const override 
		{
			std::string output;
			if constexpr (std::is_same_v<CVarType, int>)
			{
				output = IntToString(value);
			}
			else if constexpr (std::is_same_v<CVarType, bool>)
			{
				output = BoolToString(value);
			}
			else if constexpr (std::is_same_v<CVarType, float>)
			{
				output = FloatToString(value);
			}
			else if constexpr (std::is_same_v<CVarType, const char*>)
			{
				output = CStrToString(value);
			}
			return output;
		}

		[[nodiscard]] CVarType& Get() { return value; }
		[[nodiscard]] CVarType  const& Get() const { return value; }

		operator CVarType() const
		{
			return value;
		}
	private:
		CVarType value;
	};
}