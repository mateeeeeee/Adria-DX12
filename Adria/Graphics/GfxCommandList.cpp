#include "GfxCommandList.h"
#include "GfxCommandQueue.h"
#include "GfxDevice.h"
#include "../Utilities/StringUtil.h"

namespace adria
{

	GfxCommandList::GfxCommandList(GfxDevice* gfx, GfxCommandListType type /*= GfxCommandListType::Graphics*/, char const* name /*""*/)
		: gfx(gfx), cmd_queue(gfx->GetCommandQueue(type))
	{
		D3D12_COMMAND_LIST_TYPE cmd_list_type = GetCommandListType(type);
		ID3D12Device* device = gfx->GetDevice();
		HRESULT hr = device->CreateCommandAllocator(cmd_list_type, IID_PPV_ARGS(cmd_allocator.GetAddressOf()));
		BREAK_IF_FAILED(hr);

		hr = device->CreateCommandList(0, cmd_list_type, cmd_allocator, nullptr, IID_PPV_ARGS(cmd_list.GetAddressOf()));
		BREAK_IF_FAILED(hr);

		cmd_list->SetName(ToWideString(name).c_str());
		cmd_list->Close();
	}

	void GfxCommandList::ResetAllocator()
	{
		cmd_allocator->Reset();
	}

}

