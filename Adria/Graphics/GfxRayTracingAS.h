#pragma once
#include <span>
#include <memory>
#include "GfxFormat.h"

namespace adria
{
	class GfxBuffer;
	class GfxDevice;
	class GfxRayTracingBLAS;

	enum GfxRayTracingASFlagBit : Uint32
	{
		GfxRayTracingASFlag_None = 0x0,
		GfxRayTracingASFlag_AllowUpdate = 0x1,
		GfxRayTracingASFlag_AllowCompaction = 0x2,
		GfxRayTracingASFlag_PreferFastTrace = 0x4,
		GfxRayTracingASFlag_PreferFastBuild = 0x8,
		GfxRayTracingASFlag_MinimizeMemory = 0x10,
		GfxRayTracingASFlag_PerformUpdate = 0x20,
	};
	using GfxRayTracingASFlags = Uint32;

	enum GfxRayTracingInstanceFlagBit : Uint32
	{
		GfxRayTracingInstanceFlag_None = 0x0,
		GfxRayTracingInstanceFlag_CullDisable = 0x1,
		GfxRayTracingInstanceFlag_FrontCCW = 0x2,
		GfxRayTracingInstanceFlag_ForceOpaque = 0x4,
		GfxRayTracingInstanceFlag_ForceNoOpaque = 0x8,
	};

	using GfxRayTracingInstanceFlags = Uint32;

	struct GfxRayTracingGeometry
	{
		GfxBuffer* vertex_buffer;
		Uint32 vertex_buffer_offset;
		Uint32 vertex_count;
		Uint32 vertex_stride;
		GfxFormat vertex_format;

		GfxBuffer* index_buffer;
		Uint32 index_buffer_offset;
		Uint32 index_count;
		GfxFormat index_format;

		Bool opaque;
	};

	class GfxRayTracingBLAS
	{
	public:
		GfxRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);
		~GfxRayTracingBLAS();

		Uint64 GetGpuAddress() const;
		GfxBuffer const& GetBuffer() const { return *result_buffer; }
		GfxBuffer const& operator*() const { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
	};


	struct GfxRayTracingInstance
	{
		GfxRayTracingBLAS* blas;
		Float transform[4][4];
		Uint32 instance_id;
		Uint8 instance_mask;
		GfxRayTracingInstanceFlags flags;
	};

	class GfxRayTracingTLAS
	{
	public:
		GfxRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
		~GfxRayTracingTLAS();

		Uint64 GetGpuAddress() const;
		GfxBuffer const& GetBuffer() const { return *result_buffer; }
		GfxBuffer const& operator*() const { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::unique_ptr<GfxBuffer> instance_buffer;
	};
}