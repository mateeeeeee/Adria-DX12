#include "GfxQueryHeap.h"
#include "GfxDevice.h"

namespace adria
{
	GfxQueryHeap::GfxQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc) : desc(desc)
	{
		D3D12_QUERY_HEAP_DESC heap_desc{};
		heap_desc.Count = desc.count;
		heap_desc.NodeMask = 0;
		heap_desc.Type = ToD3D12QueryHeapType(desc.type);
		gfx->GetDevice()->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(query_heap.GetAddressOf()));
	}
}


