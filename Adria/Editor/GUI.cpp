#include "GUI.h"
#include "IconsFontAwesome6.h"
#include "Core/Window.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "ImGui/ImGuizmo.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace adria
{
	GUI::GUI(GfxDevice* gfx) : gfx(gfx)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigWindowsResizeFromEdges = true;
		io.ConfigViewportsNoTaskBarIcon = true;

		ImFontConfig font_config{};
		io.Fonts->AddFontFromFileTTF("Resources/Fonts/roboto/Roboto-Light.ttf", 16.0f, &font_config);
		font_config.MergeMode = true;
		ImWchar const icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		io.Fonts->AddFontFromFileTTF("Resources/Fonts/FontAwesome/" FONT_ICON_FILE_NAME_FAS, 15.0f, &font_config, icon_ranges);
		io.Fonts->Build();
		ImGui_ImplWin32_Init(gfx->GetHwnd());

		imgui_allocator = std::make_unique<GUIDescriptorAllocator>(gfx, 30, 1); //reserve first one for fonts
		GfxDescriptor handle = imgui_allocator->GetHandle(0);
		ImGui_ImplDX12_Init(gfx->GetDevice(), gfx->GetBackbufferCount(), DXGI_FORMAT_R8G8B8A8_UNORM, imgui_allocator->GetHeap(), handle, handle);
	}
	GUI::~GUI()
	{
		gfx->WaitForGPU();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void GUI::Begin() const
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		imgui_allocator->ReleaseCompletedFrames(frame_count);
	}
	void GUI::End(GfxCommandList* cmd_list) const
	{
		ImGui::Render();
		if (visible)
		{
			ID3D12DescriptorHeap* pp_heaps[] = { imgui_allocator->GetHeap() };
			cmd_list->GetNative()->SetDescriptorHeaps(ARRAYSIZE(pp_heaps), pp_heaps);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list->GetNative());
		}

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
		imgui_allocator->FinishCurrentFrame(frame_count);
		++frame_count;
	}
	void GUI::HandleWindowMessage(WindowMessage const& msg_data) const
	{
		ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(msg_data.handle),
			msg_data.msg, msg_data.wparam, msg_data.lparam);
	}

	void GUI::ToggleVisibility()
	{
		visible = !visible;
	}
	bool GUI::IsVisible() const
	{
		return visible;
	}
	
	GfxDescriptor GUI::AllocateDescriptorsGPU(uint32 count /*= 1*/) const
	{
		return imgui_allocator->Allocate(count);
	}

}