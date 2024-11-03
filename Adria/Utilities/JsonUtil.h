#pragma once
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

		template<typename T>
		ADRIA_NODISCARD T FindOr(std::string const& name, std::type_identity_t<T> const& default_value)
		{
			Bool has_field = _json.contains(name);
			if (!has_field) return default_value;
			else
			{
				json key_value_json = _json[name];
				T value;
				if (!CheckValueTypeAndAssign(key_value_json, value)) return default_value;
				else return value;
			}
		}

		template<typename T>
		ADRIA_MAYBE_UNUSED Bool Find(std::string const& name, std::type_identity_t<T>& value)
		{
			Bool has_field = _json.contains(name);
			if (!has_field) return false;
			else
			{
				json key_value_json = _json[name];
				return CheckValueTypeAndAssign(key_value_json, value);
			}
		}

		template<typename T, Uint64 N>
		ADRIA_MAYBE_UNUSED Bool FindArray(std::string const& name, T(&arr)[N])
		{
			Bool has_field = _json.contains(name);
			if (has_field)
			{
				json const key_value_json = _json[name];
				if (key_value_json.is_array() && key_value_json.size() == N)
				{
					for (Uint64 i = 0; i < N; ++i)
					{
						if (!CheckValueTypeAndAssign(key_value_json[i], arr[i])) return false;
					}
					return true;
				}
			}
			return false;
		}

		template<typename T>
		ADRIA_MAYBE_UNUSED Bool FindDynamicArray(std::string const& name, std::vector<T>& arr)
		{
			Bool has_field = _json.contains(name);
			if (has_field)
			{
				json const key_value_json = _json[name];
				if (key_value_json.is_array())
				{
					arr.clear();
					arr.resize(key_value_json.size());
					for (Uint64 i = 0; i < key_value_json.size(); ++i)
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

		template<typename T>
		static constexpr Bool CheckValueTypeAndAssign(json const& key_value_json, T& return_value)
		{
			ADRIA_ASSERT(!key_value_json.is_null());
			if (key_value_json.is_string())
			{
				if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
				{
					return_value = key_value_json.get<std::string>();
					return true;
				}
				else return false;
			}
			else if (key_value_json.is_number_float())
			{
				if constexpr (std::is_same_v<std::decay_t<T>, Float>)
				{
					return_value = key_value_json.get<Float>();
					return true;
				}
				else if constexpr (std::is_same_v<std::decay_t<T>, Float64>)
				{
					return_value = key_value_json.get<Float64>();
					return true;
				}
				else return false;
			}
			else if (key_value_json.is_number_unsigned())
			{
				if constexpr (std::is_same_v<std::decay_t<T>, Uint64>)
				{
					return_value = key_value_json.get<Uint64>();
					return true;
				}
				else if constexpr (std::is_same_v<std::decay_t<T>, Uint32>)
				{
					return_value = key_value_json.get<Uint32>();
					return true;
				}
				else return false;
			}
			else if (key_value_json.is_boolean())
			{
				if constexpr (std::is_same_v<std::decay_t<T>, Bool>)
				{
					return_value = key_value_json.get<Bool>();
					return true;
				}
				else return false;
			}
			else return false;
		}

	};
}