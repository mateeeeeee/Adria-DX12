#pragma once
#include <memory>
#include "GfxMacros.h"
#include "Utilities/Singleton.h"


namespace adria
{
	struct GfxTimestamp
	{
		Float time_in_ms;
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
		void Initialize(GfxDevice* gfx);
		void Destroy();

		void NewFrame();
		void BeginProfileScope(GfxCommandList* cmd_list, Char const* name);
		void EndProfileScope(Char const* name);
		std::vector<GfxTimestamp> GetResults();

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
			: name{ _name }, cmd_list{ _cmd_list }, active{ active }, color{ 0xffffffff }
		{
			if (active) g_GfxProfiler.BeginProfileScope(cmd_list, name.c_str());
		}
		GfxProfileScope(GfxCommandList* _cmd_list, Char const* _name, Uint32 color, Bool active)
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
