#pragma once
#include "GfxMacros.h"
#include "Utilities/Singleton.h"
#include "GfxProfilerFwd.h"
#include "Utilities/Tree.h"
#include "Utilities/LinearAllocator.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class GfxQueryHeap;
	using GfxProfilerTreeNode = typename GfxProfilerTree::NodeType;

	class GfxProfiler : public Singleton<GfxProfiler>
	{
		friend class Singleton<GfxProfiler>;
		struct Impl;
		static constexpr Uint64 FRAME_COUNT = GFX_BACKBUFFER_COUNT;
		static constexpr Uint64 MAX_PROFILES = 256;

	public:
		void Initialize(GfxDevice* gfx);
		void Destroy();

		void NewFrame();
		void BeginProfileScope(GfxCommandList* cmd_list, Char const* name);
		void EndProfileScope(GfxCommandList* cmd_list);
		GfxProfilerTree const* GetProfilerTree() const;

	private:
		std::unique_ptr<Impl> pimpl;

	private:
		GfxProfiler();
		~GfxProfiler();
	};
	#define g_GfxProfiler GfxProfiler::Get()
}
