#pragma once
#include <memory>
#include "../Core/Definitions.h"
#include "../Utilities/HashMap.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;

	using GeometryBufferHandle = uint64;
	inline constexpr GeometryBufferHandle INVALID_GEOMETRY_BUFFER_HANDLE = uint64(-1);

	class GeometryBufferCache
	{
	public:
		static GeometryBufferCache& Get();

		void Initialize(GfxDevice* _gfx);
		void Destroy();

		[[nodiscard]] GeometryBufferHandle CreateAndInitializeGeometryBuffer(GfxBuffer* staging_buffer, uint64 total_buffer_size,uint64 src_offset);
		[[nodiscard]] GfxBuffer* GetGeometryBuffer(GeometryBufferHandle handle) const;
		void DestroyGeometryBuffer(GeometryBufferHandle handle);

	private:
		GfxDevice* gfx;
		GeometryBufferHandle current_handle = INVALID_GEOMETRY_BUFFER_HANDLE;
		HashMap<GeometryBufferHandle, std::unique_ptr<GfxBuffer>> buffer_map;
	};
}