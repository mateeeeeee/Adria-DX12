#pragma once
#include <d3d12.h>

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

	class GfxQueryHeap
	{
	public:
		GfxQueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc);
		GfxQueryHeapDesc const& GetDesc() const { return desc; }
		operator ID3D12QueryHeap* () const { return query_heap.Get(); }

	private:
		Ref<ID3D12QueryHeap> query_heap;
		GfxQueryHeapDesc desc;
	};

}