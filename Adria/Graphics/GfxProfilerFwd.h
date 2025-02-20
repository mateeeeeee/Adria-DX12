#pragma once

namespace adria
{
	class GfxCommandList;

	struct TreeNodeData
	{
		GfxCommandList* cmd_list;
		Uint32 index;
		Float64 time;
	};

	template<typename T, typename Allocator>
	class Tree;
	template<Uint64 BlockSize>
	class LinearAllocator;

	static constexpr Uint32 GFX_PROFILER_TREE_ALLOCATOR_BLOCK_SIZE = 4096;
	using GfxProfilerTreeAllocator = LinearAllocator<GFX_PROFILER_TREE_ALLOCATOR_BLOCK_SIZE>;

	using GfxProfilerTree = Tree<TreeNodeData, GfxProfilerTreeAllocator>;
}

