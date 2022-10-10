#pragma once
#define _CRTDBG_MAP_ALLOC //to get more details
#include <stdlib.h>  
#include <crtdbg.h> 
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>

namespace adria
{
	typedef int (__CRTDECL* AllocHook)(int, void*, size_t, int, long, unsigned char const*, int);

	static int MemoryAllocHook(int allocType, void* userData, std::size_t size, int blockType, long requestNumber,
		const unsigned char* filename, int lineNumber)
	{
		return 1;
	}

	class MemoryDebugger
	{
	public:
		static void Checkpoint()
		{
			_CrtMemCheckpoint(&old_state);
		}

		static void CallAtExit()
		{
			std::atexit(DumpMemoryLeaks);
		}

		static void CheckLeaks()
		{
			CheckDifference();
		}

		static void SetBreak(long alloc_id)
		{
			_CrtSetBreakAlloc(alloc_id);
		}

		static void SetAllocHook(AllocHook hook)
		{
			_CrtSetAllocHook(hook);
		}

	private:

		inline static _CrtMemState old_state;
		inline static _CrtMemState new_state;
		inline static _CrtMemState diff_state;
		
	private:

		static void CheckDifference()
		{
			_CrtMemCheckpoint(&new_state);
			if (_CrtMemDifference(&diff_state, &old_state, &new_state))
			{
				OutputDebugStringA("----------- Memory Leaks Since Last Checkpoint ---------\n");
				_CrtMemDumpStatistics(&diff_state);

				_CrtDumpMemoryLeaks();
				//OutputDebugStringA("----------- Memory Allocations Since Last Checkpoint ---------\n");
				//_CrtMemDumpAllObjectsSince(&old_state);
			}
			else OutputDebugStringA("No Memory Leaks\n");
		}

		static void DumpMemoryLeaks()
		{
			OutputDebugStringA("-----------_CrtDumpMemoryLeaks ---------\n");
			_CrtDumpMemoryLeaks();
		}
	};
}