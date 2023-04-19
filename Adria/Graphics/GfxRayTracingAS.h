#pragma once
#include <span>
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
		GfxRayTracingInstanceFlag_CullDisable = 0x1,
		GfxRayTracingInstanceFlag_FrontCCW = 0x2,
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

	class GfxRayTracingBLAS
	{
	public:
		GfxRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);
		~GfxRayTracingBLAS();

		uint64 GetGpuAddress() const;
		GfxBuffer const& GetBuffer() const { return *result_buffer; }
		GfxBuffer const& operator*() const { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
	};


	struct GfxRayTracingInstance
	{
		GfxRayTracingBLAS* blas;
		float transform[4][4];
		uint32 instance_id;
		uint8 instance_mask;
		GfxRayTracingInstanceFlags flags;
	};

	class GfxRayTracingTLAS
	{
	public:
		GfxRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
		~GfxRayTracingTLAS();

		uint64 GetGpuAddress() const;
		GfxBuffer const& GetBuffer() const { return *result_buffer; }
		GfxBuffer const& operator*() const { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::unique_ptr<GfxBuffer> instance_buffer;
		void* instance_buffer_cpu_address = nullptr;
	};
}