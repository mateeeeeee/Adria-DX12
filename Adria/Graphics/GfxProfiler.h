#pragma once
#include <memory>
#include "GfxDefines.h"
#include "Utilities/Singleton.h"


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

		void Initialize(GfxDevice* gfx);
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


#if GFX_PROFILING
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
	#define AdriaGfxProfileScope(cmd_list, name) GfxProfileScope ADRIA_CONCAT(scope, __COUNTER__)(cmd_list, name)
	#define AdriaGfxProfileCondScope(cmd_list, name, active) GfxProfileScope ADRIA_CONCAT(scope, __COUNTER__)(cmd_list, name, active)
#else
	#define AdriaGfxProfileScope(cmd_list, name) 
	#define AdriaGfxProfileCondScope(cmd_list, name, active) 
#endif
}
