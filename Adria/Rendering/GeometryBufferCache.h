#pragma once
#include <memory>
#include "Core/CoreTypes.h"
#include "Utilities/HashMap.h"
#include "Utilities/Singleton.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;

	inline constexpr uint64 INVALID_GEOMETRY_BUFFER_HANDLE = -1;

	class GeometryBufferHandle
	{
	public:
		constexpr GeometryBufferHandle() = default;
		constexpr GeometryBufferHandle(uint64 h) : handle(h) {}
		GeometryBufferHandle(GeometryBufferHandle const&) = default;
		GeometryBufferHandle& operator=(GeometryBufferHandle const&) = default;
		~GeometryBufferHandle();

		operator uint64() const { return handle; }
		GeometryBufferHandle operator ++() const { return handle + 1; }

		bool IsValid() const { return handle != uint64(-1); }
	private:
		uint64 handle = INVALID_GEOMETRY_BUFFER_HANDLE;
	};

	class GeometryBufferCache : public Singleton<GeometryBufferCache>
	{
		friend class Singleton<GeometryBufferCache>;

	public:

		void Initialize(GfxDevice* _gfx);
		void Destroy();

		[[nodiscard]] GeometryBufferHandle CreateAndInitializeGeometryBuffer(GfxBuffer* staging_buffer, uint64 total_buffer_size, uint64 src_offset);
		[[nodiscard]] GfxBuffer* GetGeometryBuffer(GeometryBufferHandle handle) const;
		void DestroyGeometryBuffer(GeometryBufferHandle handle);

	private:
		GfxDevice* gfx;
		uint64 current_handle = INVALID_GEOMETRY_BUFFER_HANDLE;
		HashMap<uint64, std::unique_ptr<GfxBuffer>> buffer_map;
	};
	#define g_GeometryBufferCache GeometryBufferCache::Get()
}