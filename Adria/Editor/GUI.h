#pragma once
#include <memory>
#include "../Core/Definitions.h"
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_win32.h"
#include "../ImGui/imgui_impl_dx12.h"

namespace adria
{

	class GraphicsCoreDX12;
	class RingDescriptorAllocator;
	struct window_message_t;


	class GUI
	{
	public:
		GUI(GraphicsCoreDX12* gfx);

		~GUI();

		void Begin() const;

		void End(ID3D12GraphicsCommandList* cmd_list) const;

		void HandleWindowMessage(window_message_t const&) const;

		void ToggleVisibility();

		bool IsVisible() const;

		RingDescriptorAllocator* DescriptorAllocator() const;

	private:
		GraphicsCoreDX12* gfx;
		std::unique_ptr<RingDescriptorAllocator> imgui_allocator;
		bool visible = true;
		mutable uint64 frame_count = 0;
	};

}