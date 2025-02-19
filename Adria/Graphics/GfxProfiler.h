#pragma once
#include "GfxMacros.h"
#include "Utilities/Singleton.h"
#include "Utilities/Tree.h"
#include "Utilities/LinearAllocator.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class GfxQueryHeap;
	class GfxCommandList;

	class GfxProfiler : public Singleton<GfxProfiler>
	{
		friend class Singleton<GfxProfiler>;
		struct Impl;
		static constexpr Uint64 FRAME_COUNT = GFX_BACKBUFFER_COUNT;
		static constexpr Uint64 MAX_PROFILES = 256;

	public:
		struct TreeNodeData
		{
			GfxCommandList* cmd_list;
			Uint32 index;
			Float64 time;
		};
		using TreeAllocator = LinearAllocator<TreeNodeData, MAX_PROFILES * sizeof(TreeNodeData)>;
		using Tree = Tree<TreeNodeData, TreeAllocator>;
		using TreeNode = TreeNode<TreeNodeData, TreeAllocator>;

	public:
		void Initialize(GfxDevice* gfx);
		void Destroy();

		void NewFrame();
		void BeginProfileScope(GfxCommandList* cmd_list, Char const* name);
		void EndProfileScope(GfxCommandList* cmd_list);
		GfxProfiler::Tree const* GetProfilerTree() const;

	private:
		std::unique_ptr<Impl> pimpl;

	private:
		GfxProfiler();
		~GfxProfiler();
	};
	#define g_GfxProfiler GfxProfiler::Get()


#if GFX_PROFILING
	struct GfxProfileScope
	{
		GfxProfileScope(GfxCommandList* _cmd_list, Char const* _name, Bool active = true)
			: cmd_list{ _cmd_list }, active{ active }, color{ 0xffffffff }
		{
			if (active) g_GfxProfiler.BeginProfileScope(cmd_list, _name);
		}
		GfxProfileScope(GfxCommandList* _cmd_list, Char const* _name, Uint32 color, Bool active)
			: cmd_list{ _cmd_list }, color{ color }, active{ active }
		{
			if (active) g_GfxProfiler.BeginProfileScope(cmd_list, _name);
		}

		~GfxProfileScope()
		{
			if (active) g_GfxProfiler.EndProfileScope(cmd_list);
		}

		GfxCommandList* cmd_list;
		Bool const active;
		Uint32 color;
	};
	#define AdriaGfxProfileScope(cmd_list, name) GfxProfileScope ADRIA_CONCAT(scope, __COUNTER__)(cmd_list, name)
	#define AdriaGfxProfileCondScope(cmd_list, name, active) GfxProfileScope ADRIA_CONCAT(scope, __COUNTER__)(cmd_list, name, active)
#else
	#define AdriaGfxProfileScope(cmd_list, name) 
	#define AdriaGfxProfileCondScope(cmd_list, name, active) 
#endif
}
