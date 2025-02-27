#pragma once
#include <memory>
#include "Graphics/GfxDescriptor.h"
#include "Utilities/Singleton.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;

	inline constexpr Uint64 INVALID_GEOMETRY_BUFFER_HANDLE = -1;

	class GeometryBufferHandle
	{
		friend class ArcGeometryBufferHandle;

	public:

		~GeometryBufferHandle();
		operator Uint64() const { return handle; }
		GeometryBufferHandle operator ++() const { return handle + 1; }
		Bool IsValid() const { return handle != INVALID_GEOMETRY_BUFFER_HANDLE; }

	private:
		GeometryBufferHandle() = default;
		GeometryBufferHandle(Uint64 h) : handle(h) {}
		ADRIA_DEFAULT_COPYABLE_MOVABLE(GeometryBufferHandle)

	private:
		Uint64 handle = INVALID_GEOMETRY_BUFFER_HANDLE;
	};

	class ArcGeometryBufferHandle
	{
	public:
		ArcGeometryBufferHandle() = default;
		ArcGeometryBufferHandle(Uint64 h) : handle{ new GeometryBufferHandle(h) } {}
		~ArcGeometryBufferHandle() = default;

		operator GeometryBufferHandle&() const { return *handle; }
		Bool IsValid() const { return handle->IsValid(); }
	private:
		std::shared_ptr<GeometryBufferHandle> handle;
	};


	class GeometryBufferCache : public Singleton<GeometryBufferCache>
	{
		friend class Singleton<GeometryBufferCache>;
	public:

		void Initialize(GfxDevice* _gfx);
		void Destroy();

		ADRIA_NODISCARD ArcGeometryBufferHandle CreateAndInitializeGeometryBuffer(GfxBuffer* staging_buffer, Uint64 total_buffer_size, Uint64 src_offset);
		ADRIA_NODISCARD GfxBuffer* GetGeometryBuffer(GeometryBufferHandle& handle) const;
		ADRIA_NODISCARD GfxDescriptor GetGeometryBufferSRV(GeometryBufferHandle& handle) const;
		void DestroyGeometryBuffer(GeometryBufferHandle& handle);

	private:
		GfxDevice* gfx;
		Uint64 current_handle = INVALID_GEOMETRY_BUFFER_HANDLE;
		std::unordered_map<Uint64, std::unique_ptr<GfxBuffer>> buffer_map;
		std::unordered_map<Uint64, GfxDescriptor> buffer_srv_map;
	};
	#define g_GeometryBufferCache GeometryBufferCache::Get()
}