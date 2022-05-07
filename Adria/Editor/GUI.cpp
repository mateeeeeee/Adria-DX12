#include "GUI.h"
#include "../Core/Window.h"
#include "../Graphics/GraphicsCoreDX12.h"
#include "../ImGui/ImGuizmo.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace adria
{
	GUI::GUI(GraphicsCoreDX12* gfx) : gfx(gfx)
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


		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 30;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		
		imgui_allocator = std::make_unique<RingDescriptorAllocator>(gfx->GetDevice(), desc, 1); //reserve first for fonts

		ImGui_ImplWin32_Init(Window::Handle());
		
		ImGui_ImplDX12_Init(gfx->GetDevice(), gfx->BackbufferCount(),
			DXGI_FORMAT_R10G10B10A2_UNORM, imgui_allocator->Heap(),
			imgui_allocator->GetFirstHandle(),
			imgui_allocator->GetFirstHandle());

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.ScrollbarSize = 20.0f;
		style.FramePadding = ImVec2(5, 5);
		style.ItemSpacing = ImVec2(6, 5);
		style.WindowMenuButtonPosition = ImGuiDir_Right;
		style.WindowRounding = 2.0f;
		style.FrameRounding = 2.0f;
		style.PopupRounding = 2.0f;
		style.GrabRounding = 2.0f;
		style.ScrollbarRounding = 2.0f;
		style.Alpha = 1.0f;
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

	void GUI::End(ID3D12GraphicsCommandList* cmd_list) const
	{
		ImGui::Render();

		if (visible)
		{
			ID3D12DescriptorHeap* pp_heaps[] = { imgui_allocator->Heap() };
			cmd_list->SetDescriptorHeaps(_countof(pp_heaps), pp_heaps);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list);
		}

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		imgui_allocator->FinishCurrentFrame(frame_count);

		++frame_count;

	}
	void GUI::HandleWindowMessage(window_message_t const& msg_data) const
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
	RingDescriptorAllocator* GUI::DescriptorAllocator() const
	{
		return imgui_allocator.get();
	}
}