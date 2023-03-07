#include "GeometryBufferCache.h"
#include "../Graphics/Buffer.h"

namespace adria
{

	GeometryBufferCache& GeometryBufferCache::Get()
	{
		static GeometryBufferCache cache;
		return cache;
	}

	void GeometryBufferCache::Initialize(GraphicsDevice* _gfx)
	{
		gfx = _gfx;
	}

	void GeometryBufferCache::Destroy()
	{
		buffer_map.clear();
		gfx = nullptr;
	}

	GeometryBufferHandle GeometryBufferCache::CreateAndInitializeGeometryBuffer(uint64 total_buffer_size, void* resource, uint64 src_offset)
	{
		BufferDesc desc{};
		desc.size = total_buffer_size;
		desc.bind_flags = EBindFlag::ShaderResource;
		desc.misc_flags = EBufferMiscFlag::BufferRaw;
		desc.resource_usage = EResourceUsage::Default;

		++current_handle;
		buffer_map[current_handle] = std::make_unique<Buffer>(gfx, desc);
		if(resource != nullptr) gfx->GetDefaultCommandList()->CopyBufferRegion(buffer_map[current_handle]->GetNative(), 0, (ID3D12Resource*)resource, src_offset, total_buffer_size);
		return current_handle;
	}

	void GeometryBufferCache::DestroyGeometryBuffer(GeometryBufferHandle handle) //TODO: when to call this?
	{
		if (auto it = buffer_map.find(handle); it != buffer_map.end())
		{
			it->second = nullptr;
			buffer_map.erase(it->first);
		}
	}

}

