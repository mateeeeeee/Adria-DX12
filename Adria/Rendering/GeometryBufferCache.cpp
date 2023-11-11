#include "GeometryBufferCache.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"

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

	ArcGeometryBufferHandle GeometryBufferCache::CreateAndInitializeGeometryBuffer(GfxBuffer* staging_buffer, uint64 total_buffer_size, uint64 src_offset)
	{
		GfxBufferDesc desc{};
		desc.size = total_buffer_size;
		desc.bind_flags = GfxBindFlag::ShaderResource;
		desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
		desc.resource_usage = GfxResourceUsage::Default;

		++current_handle;
		buffer_map[current_handle] = gfx->CreateBuffer(desc);
		if(staging_buffer) gfx->GetCommandList()->CopyBuffer(*buffer_map[current_handle], 0, *staging_buffer, src_offset, total_buffer_size);
		buffer_srv_map[current_handle] = gfx->CreateBufferSRV(buffer_map[current_handle].get());
		return current_handle;
	}

	void GeometryBufferCache::DestroyGeometryBuffer(GeometryBufferHandle& handle)
	{
		if (buffer_map.empty()) return;
		if (auto it = buffer_map.find(handle); it != buffer_map.end())
		{
			it->second = nullptr;
			buffer_map.erase(it->first);
		}
	}

	GfxBuffer* GeometryBufferCache::GetGeometryBuffer(GeometryBufferHandle& handle) const
	{
		if (!handle.IsValid()) return nullptr;

		if (auto it = buffer_map.find(handle); it != buffer_map.end())
		{
			return it->second.get();
		}
		else return nullptr;
	}

	GfxDescriptor GeometryBufferCache::GetGeometryBufferSRV(GeometryBufferHandle& handle) const
	{
		if (!handle.IsValid()) return GfxDescriptor{};

		if (auto it = buffer_srv_map.find(handle); it != buffer_srv_map.end())
		{
			return it->second;
		}
		else return GfxDescriptor{};
	}

	GeometryBufferHandle::~GeometryBufferHandle()
	{
		if (IsValid()) g_GeometryBufferCache.DestroyGeometryBuffer(*this);
	}

}

