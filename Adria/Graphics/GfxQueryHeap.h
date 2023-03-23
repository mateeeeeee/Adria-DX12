#pragma once
#include <d3d12.h>
#include "../Core/Definitions.h"
#include "../Utilities/AutoRefCountPtr.h"

namespace adria
{
	class GfxDevice;

	enum class GfxQueryType : uint8
	{
		Occlusion,
		BinaryOcclusion,
		Timestamp,
		PipelineStatistics
	};
	struct GfxQueryHeapDesc
	{
		uint32 count;
		GfxQueryType type;
	};

	constexpr D3D12_QUERY_TYPE ToD3D12QueryType(GfxQueryType query_type)
	{
		switch (query_type)
		{
		case GfxQueryType::Timestamp:
			return D3D12_QUERY_TYPE_TIMESTAMP;
		case GfxQueryType::Occlusion:
			return D3D12_QUERY_TYPE_OCCLUSION;
		case GfxQueryType::BinaryOcclusion:
			return D3D12_QUERY_TYPE_BINARY_OCCLUSION;
		case GfxQueryType::PipelineStatistics:
			return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
		}
		return D3D12_QUERY_TYPE_TIMESTAMP;
	}
	constexpr D3D12_QUERY_HEAP_TYPE ToD3D12QueryHeapType(GfxQueryType query_type)
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

	class GfxQueryHeap
	{
	public:
		GfxQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc);
		GfxQueryHeapDesc GetDesc() const { return desc; }
		operator ID3D12QueryHeap* () const { return query_heap.Get(); }

	private:
		ArcPtr<ID3D12QueryHeap> query_heap;
		GfxQueryHeapDesc desc;
	};

}