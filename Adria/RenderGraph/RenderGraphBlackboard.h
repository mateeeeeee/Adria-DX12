#pragma once
#include <typeindex>
#include <memory>
#include "Utilities/TemplatesUtil.h"

namespace adria
{

	class RenderGraphBlackboard
	{
	public:
		RenderGraphBlackboard() = default;
		ADRIA_NONCOPYABLE(RenderGraphBlackboard)
		~RenderGraphBlackboard() = default;

		template<typename T>
		T& Add(T&& data)
		{
			return Create<std::remove_cvref_t<T>>(std::forward<T>(data));
		}

		template<typename T, typename... Args> requires std::is_constructible_v<T, Args...>
		T& Create(Args&&... args)
		{
			static_assert(std::is_trivial_v<T> && std::is_standard_layout_v<T>);
			ADRIA_ASSERT(board_data.find(typeid(T)) == board_data.end() && "Cannot create same type more than once in blackboard!");
			board_data[typeid(T)] = std::make_unique<uint8[]>(sizeof(T));
			T* data_entry = reinterpret_cast<T*>(board_data[typeid(T)].get());
			*data_entry = T{ std::forward<Args>(args)... };
			return *data_entry;
		}

		template<typename T>
		T const* TryGet() const
		{
			if (auto it = board_data.find(typeid(T)); it != board_data.end())
			{
				return reinterpret_cast<T const*>(it->second.get());
			}
			else return nullptr;
		}

		template<typename T>
		T const& Get() const
		{
			T const* p_data = TryGet<T>();
			ADRIA_ASSERT(p_data != nullptr);
			return *p_data;
		}

		void Clear()
		{
			board_data.clear();
		}

	private:
		std::unordered_map<std::type_index, std::unique_ptr<uint8[]>> board_data;
	};

	using RGBlackboard = RenderGraphBlackboard;

}