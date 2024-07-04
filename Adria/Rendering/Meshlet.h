#pragma once

namespace adria
{
	static constexpr uint64 MESHLET_MAX_TRIANGLES = 124;
	static constexpr uint64 MESHLET_MAX_VERTICES = 64;

	struct MeshletTriangle
	{
		uint32 V0 : 10;
		uint32 V1 : 10;
		uint32 V2 : 10;
		uint32	  : 2;
	};

	struct Meshlet
	{
		float center[3];
		float radius;

		uint32 vertex_count;
		uint32 triangle_count;

		uint32 vertex_offset;
		uint32 triangle_offset;
	};
}