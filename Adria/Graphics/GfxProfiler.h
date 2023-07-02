#pragma once
#include <memory>
#include "GfxDefines.h"
#include "Core/CoreTypes.h"
#include "Utilities/Singleton.h"
#if GFX_PROFILING_USE_TRACY
#include "tracy/TracyD3D12.hpp"
#endif

namespace adria
{
	struct GfxTimestamp
	{
		float time_in_ms;
		std::string name;
	};

	class GfxDevice;
	class GfxBuffer;
	class GfxQueryHeap;
	class GfxCommandList;

	class GfxProfiler : public Singleton<GfxProfiler>
	{
		friend class Singleton<GfxProfiler>;
		struct Impl;

	public:

		void Init(GfxDevice* gfx);
		void Destroy();

		void NewFrame();
		void BeginProfileScope(GfxCommandList* cmd_list, char const* name);
		void EndProfileScope(char const* name);
		std::vector<GfxTimestamp> GetResults();

	private:
		std::unique_ptr<Impl> pimpl;

	private:
		GfxProfiler();
		~GfxProfiler();
	};
	#define g_GfxProfiler GfxProfiler::Get()

//#define TracyD3D12Zone(ctx, cmdList, name)
//#define TracyD3D12ZoneC(ctx, cmdList, name, color)
//#define TracyD3D12NamedZone(ctx, varname, cmdList, name, active)
//#define TracyD3D12NamedZoneC(ctx, varname, cmdList, name, color, active)
//#define TracyD3D12ZoneTransient(ctx, varname, cmdList, name, active)

#if GFX_PROFILING
#if GFX_PROFILING_USE_TRACY
#define AdriaGfxProfileScope(cmd_list, name) 
#define AdriaGfxProfileScopeColored(cmd_list, name, color) 
#define AdriaGfxProfileCondScope(cmd_list, name, active) 
#define AdriaGfxProfileCondScopeColored(cmd_list, name, active, color)
#else
struct GfxProfileScope
{
	GfxProfileScope(GfxCommandList* _cmd_list, char const* _name, bool active = true)
		: name{ _name }, cmd_list{ _cmd_list }, active{ active }, color{ 0xffffffff }
	{
		if (active) g_GfxProfiler.BeginProfileScope(cmd_list, name.c_str());
	}
	GfxProfileScope(GfxCommandList* _cmd_list, char const* _name, uint32 color, bool active)
		: name{ _name }, cmd_list{ _cmd_list }, color{ color }, active{ active }
	{
		if (active) g_GfxProfiler.BeginProfileScope(cmd_list, name.c_str());
	}

	~GfxProfileScope()
	{
		if (active) g_GfxProfiler.EndProfileScope(name.c_str());
	}

	GfxCommandList* cmd_list;
	std::string name;
	bool const active;
	uint32 color;
};
#define AdriaGfxProfileScope(cmd_list, name) GfxProfileScope gfx_scope(cmd_list, name)
#define AdriaGfxProfileScopeColored(cmd_list, name, color) GfxProfileScope gfx_scope(cmd_list, name, color, true)
#define AdriaGfxProfileCondScope(cmd_list, name, active) GfxProfileScope gfx_scope(cmd_list, name, active)
#define AdriaGfxProfileCondScopeColored(cmd_list, name, active, color) GfxProfileScope gfx_scope(cmd_list, name, color, active)
#endif
#else
#define AdriaGfxProfileScope(cmd_list, name) 
#define AdriaGfxProfileScopeColored(cmd_list, name, color) 
#define AdriaGfxProfileCondScope(cmd_list, name, active) 
#define AdriaGfxProfileCondScopeColored(cmd_list, name, active, color)
#endif
}
