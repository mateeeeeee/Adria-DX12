#pragma once
#define _CRTDBG_MAP_ALLOC //to get more details
#include <stdlib.h>  
#include <crtdbg.h> 
#include "../Core/Definitions.h"
#include <windows.h>
#include <memory>

namespace adria
{
	
	class MemoryLeak
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