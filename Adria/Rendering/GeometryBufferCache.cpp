#include "GeometryBufferCache.h"
#include "../Graphics/GfxBuffer.h"
#include "../Graphics/GfxDevice.h"
#include "../Graphics/GfxCommandList.h"

namespace adria
{
	void GeometryBufferCache::Initialize(GfxDevice* _gfx)
	{
		gfx = _gfx;
	}

	void GeometryBufferCache::Destroy()
	{
		buffer_map.clear();
		gfx = nullptr;
	}

	GeometryBufferHandle GeometryBufferCache::CreateAndInitializeGeometryBuffer(GfxBuffer* staging_buffer, uint64 total_buffer_size, uint64 src_offset)
	{
		GfxBufferDesc desc{};
		desc.size = total_buffer_size;
		desc.bind_flags = GfxBindFlag::ShaderResource;
		desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
		desc.resource_usage = GfxResourceUsage::Default;

		++current_handle;
		buffer_map[current_handle] = std::make_unique<GfxBuffer>(gfx, desc);
		if(staging_buffer) gfx->GetGraphicsCommandList()->CopyBuffer(*buffer_map[current_handle], 0, *staging_buffer, src_offset, total_buffer_size);
		return current_handle;
	}

	void GeometryBufferCache::DestroyGeometryBuffer(GeometryBufferHandle handle) 
	{
		if (auto it = buffer_map.find(handle); it != buffer_map.end())
		{
			it->second = nullptr;
			buffer_map.erase(it->first);
		}
	}

	GfxBuffer* GeometryBufferCache::GetGeometryBuffer(GeometryBufferHandle handle) const
	{
		if (auto it = buffer_map.find(handle); it != buffer_map.end())
		{
			return it->second.get();
		}
		else return nullptr;
	}

	GeometryBufferHandle::~GeometryBufferHandle()
	{
		GeometryBufferCache::Get().DestroyGeometryBuffer(*this);
	}

}

