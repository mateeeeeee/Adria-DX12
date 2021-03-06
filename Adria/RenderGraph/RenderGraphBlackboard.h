#pragma once
#include <typeindex>
#include <memory>
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Utilities/TemplatesUtil.h"
#include "../Utilities/HashMap.h"


namespace adria
{

	class RenderGraphBlackboard
	{
	public:
		RenderGraphBlackboard() = default;
		~RenderGraphBlackboard() = default;
		RenderGraphBlackboard(RenderGraphBlackboard const&) = delete;
		RenderGraphBlackboard& operator=(RenderGraphBlackboard const&) = delete;

		template<typename T>
		T& Add(T&& data)
		{
			return Create<std::remove_cvref_t<T>>(std::forward<T>(data));
		}

		template<typename T, typename... Args> requires std::is_constructible_v<T, Args...>
		T& Create(Args&&... args)
		{
			ADRIA_ASSERT(board_data.find(typeid(T)) == board_data.end() && "Cannot create same type more than once in blackboard!");
			board_data[typeid(T)] = std::make_unique<uint8[]>(sizeof(T));
			T* data_entry = reinterpret_cast<T*>(board_data[typeid(T)].get());
			*data_entry = T{ std::forward<Args>(args)... };
			return *data_entry;
		}

		template<typename T>
		T const* Get() const
		{
			if (auto it = board_data.find(typeid(T)); it != board_data.end())
			{
				return reinterpret_cast<T const*>(it->second.get());
			}
			else return nullptr;
		}

		template<typename T>
		T const& GetChecked() const
		{
			T const* p_data = Get<T>();
			ADRIA_ASSERT(p_data != nullptr);
			return *p_data;
		}

	private:
		HashMap<std::type_index, std::unique_ptr<uint8[]>> board_data;
	};

	using RGBlackboard = RenderGraphBlackboard;

}