#pragma once

namespace adria
{
	static constexpr Uint64 MESHLET_MAX_TRIANGLES = 124;
	static constexpr Uint64 MESHLET_MAX_VERTICES = 64;

	struct MeshletTriangle
	{
		Uint32 V0 : 10;
		Uint32 V1 : 10;
		Uint32 V2 : 10;
		Uint32	  : 2;
	};

	struct Meshlet
	{
		Float center[3];
		Float radius;

		Uint32 vertex_count;
		Uint32 triangle_count;

		Uint32 vertex_offset;
		Uint32 triangle_offset;
	};
}