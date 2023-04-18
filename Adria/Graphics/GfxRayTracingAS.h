#pragma once
#include <vector>
#include <memory>
#include "GfxFormat.h"

namespace adria
{
	class GfxBuffer;
	class GfxDevice;
	class GfxRayTracingBLAS;

	enum GfxRayTracingASFlagBit : uint32
	{
		GfxRayTracingASFlag_None = 0x0,
		GfxRayTracingASFlag_AllowUpdate = 0x1,
		GfxRayTracingASFlag_AllowCompaction = 0x2,
		GfxRayTracingASFlag_PreferFastTrace = 0x4,
		GfxRayTracingASFlag_PreferFastBuild = 0x8,
		GfxRayTracingASFlag_MinimizeMemory = 0x10,
		GfxRayTracingASFlag_PerformUpdate = 0x20,
	};
	using GfxRayTracingASFlags = uint32;

	enum GfxRayTracingInstanceFlagBit : uint32
	{
		GfxRayTracingInstanceFlag_None = 0x0,
		GfxRayTracingInstanceFlag_DisableCull = 0x1,
		GfxRayTracingInstanceFlag_FrontFaceCCW = 0x2,
		GfxRayTracingInstanceFlag_ForceOpaque = 0x4,
		GfxRayTracingInstanceFlag_ForceNoOpaque = 0x8,
	};

	using GfxRayTracingInstanceFlags = uint32;

	struct GfxRayTracingGeometry
	{
		GfxBuffer* vertex_buffer;
		uint32 vertex_buffer_offset;
		uint32 vertex_count;
		uint32 vertex_stride;
		GfxFormat vertex_format;

		GfxBuffer* index_buffer;
		uint32 index_buffer_offset;
		uint32 index_count;
		GfxFormat index_format;

		bool opaque;
	};

	struct GfxRayTracingInstance
	{
		GfxRayTracingBLAS* blas;
		float transform[12]; 
		uint32 instance_id;
		uint8 instance_mask;
		GfxRayTracingInstanceFlags flags;
	};

	struct GfxRayTracingBLASDesc
	{
		std::vector<GfxRayTracingGeometry> geometries;
		GfxRayTracingASFlags flags;
	};

	struct GfxRayTracingTLASDesc
	{
		uint32 instance_count;
		GfxRayTracingASFlags flags;
	};

	class GfxRayTracingBLAS 
	{
	public:
		GfxRayTracingBLAS(GfxDevice* gfx, GfxRayTracingBLASDesc const& desc)
		{

		}
		~GfxRayTracingBLAS()
		{

		}

		GfxRayTracingBLASDesc const& GetDesc() const { return desc; }
		uint64 GetGpuAddress() const;

	private:
		GfxRayTracingBLASDesc desc;
		std::unique_ptr<GfxBuffer> as_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
	};

	class GfxRayTracingTLAS
	{
	public:
		GfxRayTracingTLAS(GfxDevice* gfx, GfxRayTracingTLASDesc const& desc)
		{

		}
		~GfxRayTracingTLAS()
		{

		}

		GfxRayTracingTLASDesc const& GetDesc() const { return desc; }
		uint64 GetGpuAddress() const;

	private:
		GfxRayTracingTLASDesc desc;
		std::unique_ptr<GfxBuffer> as_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::unique_ptr<GfxBuffer> instance_buffer;
		void* instance_buffer_cpu_address = nullptr;
	};
}