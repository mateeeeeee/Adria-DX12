#pragma once
#include <memory>
#include "../Core/Definitions.h"
#include "../Utilities/HashMap.h"

namespace adria
{
	class GraphicsDevice;
	class Buffer;

	using GeometryBufferHandle = uint64;
	inline constexpr GeometryBufferHandle INVALID_GEOMETRY_BUFFER_HANDLE = uint64(-1);

	class GeometryBufferCache
	{
	public:
		static GeometryBufferCache& Get();

		void Initialize(GraphicsDevice* _gfx);
		void Destroy();

		[[nodiscard]] GeometryBufferHandle CreateAndInitializeGeometryBuffer(uint64 total_buffer_size, void* resource, uint64 src_offset);
		[[nodiscard]] Buffer* GetGeometryBuffer(GeometryBufferHandle handle) const;
		void DestroyGeometryBuffer(GeometryBufferHandle handle);

	private:
		GraphicsDevice* gfx;
		GeometryBufferHandle current_handle = INVALID_GEOMETRY_BUFFER_HANDLE;
		HashMap<GeometryBufferHandle, std::unique_ptr<Buffer>> buffer_map;
	};
}