#pragma once
#include <memory>
#include "../Core/CoreTypes.h"
#include "../Utilities/HashMap.h"
#include "../Utilities/Singleton.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;

	inline constexpr uint64 INVALID_GEOMETRY_BUFFER_HANDLE_VALUE = uint64(-1);

	class GeometryBufferHandle
	{
	public:
		GeometryBufferHandle() = default;
		GeometryBufferHandle(uint64 h)
		{
			handle = std::make_shared<uint64>(h);
		}
		GeometryBufferHandle(GeometryBufferHandle const&) = default;
		GeometryBufferHandle& operator=(GeometryBufferHandle const&) = default;
		~GeometryBufferHandle();

		operator uint64() const { return *handle; }
	private:
		std::shared_ptr<uint64> handle = nullptr;
	};

	class GeometryBufferCache : public Singleton<GeometryBufferCache>
	{
		friend class Singleton<GeometryBufferCache>;

	public:
		
		void Initialize(GfxDevice* _gfx);
		void Destroy();

		[[nodiscard]] GeometryBufferHandle CreateAndInitializeGeometryBuffer(GfxBuffer* staging_buffer, uint64 total_buffer_size,uint64 src_offset);
		[[nodiscard]] GfxBuffer* GetGeometryBuffer(GeometryBufferHandle handle) const;
		void DestroyGeometryBuffer(GeometryBufferHandle handle);

	private:
		GfxDevice* gfx;
		uint64 current_handle = INVALID_GEOMETRY_BUFFER_HANDLE_VALUE;
		HashMap<uint64, std::unique_ptr<GfxBuffer>> buffer_map;
	};
}