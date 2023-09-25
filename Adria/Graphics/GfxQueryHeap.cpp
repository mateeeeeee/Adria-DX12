#include "GfxQueryHeap.h"
#include "GfxDevice.h"

namespace adria
{
	static constexpr D3D12_QUERY_HEAP_TYPE ToD3D12QueryHeapType(GfxQueryType query_type)
	{
		switch (query_type)
		{
		case GfxQueryType::Timestamp:
			return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		case GfxQueryType::Occlusion:
		case GfxQueryType::BinaryOcclusion:
			return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		case GfxQueryType::PipelineStatistics:
			return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
		}
		return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	}

	GfxQueryHeap::GfxQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc) : desc(desc)
	{
		D3D12_QUERY_HEAP_DESC heap_desc{};
		heap_desc.Count = desc.count;
		heap_desc.NodeMask = 0;
		heap_desc.Type = ToD3D12QueryHeapType(desc.type);
		gfx->GetDevice()->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(query_heap.GetAddressOf()));
	}
}


