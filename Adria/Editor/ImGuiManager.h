#pragma once
#include <memory>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx12.h"
#include "Graphics/GfxDescriptor.h"


namespace adria
{

	class GfxDevice;
	template<bool>
	class GfxRingDescriptorAllocator;
	class GfxCommandList;

	using GUIDescriptorAllocator = GfxRingDescriptorAllocator<false>;

	struct WindowEventData;

	class ImGuiManager
	{
	public:
		explicit ImGuiManager(GfxDevice* gfx);
		~ImGuiManager();

		void Begin() const;
		void End(GfxCommandList* cmd_list) const;

		void OnWindowEvent(WindowEventData const&) const;

		void ToggleVisibility();
		bool IsVisible() const;

		GfxDescriptor AllocateDescriptorsGPU(uint32 count = 1) const;

	private:
		GfxDevice* gfx;
		std::string ini_file;
		std::unique_ptr<GUIDescriptorAllocator> imgui_allocator;
		bool visible = true;
		mutable uint64 frame_count = 0;
	};

}