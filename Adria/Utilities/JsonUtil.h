#pragma once
#include "Logging/Logger.h"
#include "Core/Defines.h"
#include "nlohmann/json.hpp"


using json = nlohmann::json;

namespace adria
{

	class JsonParams
	{
	public:
		JsonParams(json const& json_object) : _json(json_object)
		{
			ADRIA_ASSERT(!_json.is_null());
		}
		JsonParams(json&& json_object) : _json(std::move(json_object))
		{
			ADRIA_ASSERT(!_json.is_null());
		}

		json FindJson(std::string const& name)
		{
			json _return_json = _json.value(name, json::object());
			return _return_json.is_object() ? _return_json : json::object();
		}

		json FindJsonArray(std::string const& name)
		{
			json _return_json = _json.value(name, json::array());
			return _return_json.is_array() ? _return_json : json::array();
		}

		//type_identity_t avoids deducing RequiredType from default_value so the user must explicitly provide template param

		template<typename RequiredType> 
		[[nodiscard]] RequiredType FindOr(std::string const& name, std::type_identity_t<RequiredType> const& default_value)
		{
			bool has_field = _json.contains(name);
			if (!has_field) return default_value;
			else
			{
				json key_value_json = _json[name];
				RequiredType value;
				if (!CheckValueTypeAndAssign(key_value_json, value)) return default_value;
				else return value;
			}
		}

		template<typename RequiredType>
		[[maybe_unused]] bool Find(std::string const& name, std::type_identity_t<RequiredType>& value)
		{
			bool has_field = _json.contains(name);
			if (!has_field) return false;
			else
			{
				json key_value_json = _json[name];
				return CheckValueTypeAndAssign(key_value_json, value);
			}
		}

		template<typename RequiredType, size_t N>
		[[maybe_unused]] bool FindArray(std::string const& name, RequiredType(&arr)[N])
		{
			bool has_field = _json.contains(name);
			if (has_field)
			{
				json const key_value_json = _json[name];
				if (key_value_json.is_array() && key_value_json.size() == N)
				{
					for (size_t i = 0; i < N; ++i)
					{
						if (!CheckValueTypeAndAssign(key_value_json[i], arr[i])) return false;
					}
					return true;
				}
			}
			return false;
		}

		template<typename RequiredType>
		[[maybe_unused]] bool FindDynamicArray(std::string const& name, std::vector<RequiredType>& arr)
		{
			bool has_field = _json.contains(name);
			if (has_field)
			{
				json const key_value_json = _json[name];
				if (key_value_json.is_array())
				{
					arr.clear();
					arr.resize(key_value_json.size());
					for (size_t i = 0; i < key_value_json.size(); ++i)
					{
						if (!CheckValueTypeAndAssign(key_value_json[i], arr[i])) return false;
					}
					return true;
				}
			}
			return false;
		}

	private:
		json _json;

	private:

		template<typename RequiredType>
		static constexpr bool CheckValueTypeAndAssign(json const& key_value_json, RequiredType& return_value)
		{
			ADRIA_ASSERT(!key_value_json.is_null());
			if (key_value_json.is_string())
			{
				if constexpr (std::is_same_v<std::decay_t<RequiredType>, std::string>)
				{
					return_value = key_value_json.get<std::string>();
					return true;
				}
				else return false;
			}
			else if (key_value_json.is_number_float())
			{
				if constexpr (std::is_same_v<std::decay_t<RequiredType>, float>)
				{
					return_value = key_value_json.get<float>();
					return true;
				}
				else if constexpr (std::is_same_v<std::decay_t<RequiredType>, double>)
				{
					return_value = key_value_json.get<double>();
					return true;
				}
				else return false;
			}
			else if (key_value_json.is_number_unsigned())
			{
				if constexpr (std::is_same_v<std::decay_t<RequiredType>, size_t>)
				{
					return_value = key_value_json.get<size_t>();
					return true;
				}
				else if constexpr (std::is_same_v<std::decay_t<RequiredType>, uint32_t>)
				{
					return_value = key_value_json.get<uint32_t>();
					return true;
				}
				else return false;
			}
			else if (key_value_json.is_boolean())
			{
				if constexpr (std::is_same_v<std::decay_t<RequiredType>, bool>)
				{
					return_value = key_value_json.get<bool>();
					return true;
				}
				else return false;
			}
			else return false;
		}

	};
}