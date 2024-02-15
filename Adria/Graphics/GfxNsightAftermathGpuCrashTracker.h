#pragma once
#include <mutex>
#include "GFSDK_Aftermath_Defines.h"
#include "GFSDK_Aftermath_GpuCrashDump.h"
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"

namespace adria
{
	class GfxNsightAftermathGpuCrashTracker
	{
		static bool operator<(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& lhs, const GFSDK_Aftermath_ShaderDebugInfoIdentifier& rhs)
		{
			if (lhs.id[0] == rhs.id[0])
			{
				return lhs.id[1] < rhs.id[1];
			}
			return lhs.id[0] < rhs.id[0];
		}
		static bool operator<(const GFSDK_Aftermath_ShaderBinaryHash& lhs, const GFSDK_Aftermath_ShaderBinaryHash& rhs)
		{
			return lhs.hash < rhs.hash;
		}
		static bool operator<(const GFSDK_Aftermath_ShaderDebugName& lhs, const GFSDK_Aftermath_ShaderDebugName& rhs)
		{
			return strncmp(lhs.name, rhs.name, sizeof(lhs.name)) < 0;
		}

	public:
		GfxNsightAftermathGpuCrashTracker()
		{
			GFSDK_Aftermath_Result result = GFSDK_Aftermath_EnableGpuCrashDumps(
				GFSDK_Aftermath_Version_API,
				GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX,
				GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,	// Let the Nsight Aftermath library cache shader debug information.
				GpuCrashDumpCallback,												// Register callback for GPU crash dumps.
				ShaderDebugInfoCallback,											// Register callback for shader debug information.
				CrashDumpDescriptionCallback,										// Register callback for GPU crash dump description.
				NULL,																// Register callback for resolving application-managed markers.
				this);																// Set the GpuCrashTracker object as user data for the above callbacks.

			initialized = (result == GFSDK_Aftermath_Result_Success);
		}
		~GfxNsightAftermathGpuCrashTracker()
		{
			if(initialized) GFSDK_Aftermath_DisableGpuCrashDumps();
		}


	private:
		bool initialized = false;
		mutable std::mutex m_mutex;
		std::map<GFSDK_Aftermath_ShaderDebugInfoIdentifier, std::vector<uint8>> shader_debug_info_map;

	private:
		void OnCrashDump(const void* gpu_crash_dump_data, const uint32 gpu_crash_dump_size)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			WriteGpuCrashDumpToFile(gpu_crash_dump_data, gpu_crash_dump_size);
		}
		void OnShaderDebugInfo(const void* shader_debug_info, const uint32 shader_debug_info_size)
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier{};
			GFSDK_Aftermath_Result result = GFSDK_Aftermath_GetShaderDebugInfoIdentifier(
				GFSDK_Aftermath_Version_API,
				shader_debug_info,
				shader_debug_info_size,
				&identifier);
			ADRIA_ASSERT(result == GFSDK_Aftermath_Result_Success);

			// Store information for decoding of GPU crash dumps with shader address mapping // from within the application.
			std::vector<uint8> data((uint8*)shader_debug_info, (uint8*)shader_debug_info + shader_debug_info_size);
			shader_debug_info_map[identifier].swap(data);
			// Write to file for later in-depth analysis of crash dumps with Nsight Graphics
			WriteShaderDebugInformationToFile(identifier, shader_debug_info, shader_debug_info_size);
		}
		void OnDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description)
		{
			add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "Adria-DX12");
			add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "v1.0");
			add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined, "Nvidia Nsight Aftermath crash dump");
		}

		void OnShaderDebugInfoLookup(GFSDK_Aftermath_ShaderDebugInfoIdentifier const& identifier, PFN_GFSDK_Aftermath_SetData setShaderDebugInfo) const
		{
			//TODO
		}
		// Handler for shader lookup callbacks.
		// This is used by the JSON decoder for mapping shader instruction
		// addresses to DXIL lines or HLSL source lines.
		// NOTE: If the application loads stripped shader binaries (-Qstrip_debug),
		// Aftermath will require access to both the stripped and the not stripped
		// shader binaries.
		void OnShaderLookup(GFSDK_Aftermath_ShaderBinaryHash const& shaderHash, PFN_GFSDK_Aftermath_SetData setShaderBinary) const
		{
			//TODO
		}

		// Handler for shader source debug info lookup callbacks.
		// This is used by the JSON decoder for mapping shader instruction addresses to
		// HLSL source lines, if the shaders used by the application were compiled with
		// separate debug info data files.
		void OnShaderSourceDebugInfoLookup(GFSDK_Aftermath_ShaderDebugName const& shaderDebugName, PFN_GFSDK_Aftermath_SetData setShaderBinary) const
		{
			//TODO
		}

		static void GpuCrashDumpCallback(void const* gpu_crash_dump_data,  uint32 gpu_crash_dump_size, void* user_data)
		{
			GfxNsightAftermathGpuCrashTracker* gpu_crash_tracker = reinterpret_cast<GfxNsightAftermathGpuCrashTracker*>(user_data);
			gpu_crash_tracker->OnCrashDump(gpu_crash_dump_data, gpu_crash_dump_size);
		}
		static void ShaderDebugInfoCallback(void const* shader_debug_info, uint32 shader_debug_info_size, void* user_data)
		{
			GfxNsightAftermathGpuCrashTracker* gpu_crash_tracker = reinterpret_cast<GfxNsightAftermathGpuCrashTracker*>(user_data);
			gpu_crash_tracker->OnShaderDebugInfo(shader_debug_info, shader_debug_info_size);
		}

		static void CrashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description, void* user_data)
		{
			GfxNsightAftermathGpuCrashTracker* gpu_crash_tracker = reinterpret_cast<GfxNsightAftermathGpuCrashTracker*>(user_data);
			gpu_crash_tracker->OnDescription(add_description);
		}

		static void ShaderDebugInfoLookupCallback(GFSDK_Aftermath_ShaderDebugInfoIdentifier const* identifier, PFN_GFSDK_Aftermath_SetData set_shader_debug_info, void* user_data)
		{
			GfxNsightAftermathGpuCrashTracker* gpu_crash_tracker = reinterpret_cast<GfxNsightAftermathGpuCrashTracker*>(user_data);
			gpu_crash_tracker->OnShaderDebugInfoLookup(*identifier, set_shader_debug_info);
		}

		static void ShaderLookupCallback(GFSDK_Aftermath_ShaderBinaryHash const* shader_hash, PFN_GFSDK_Aftermath_SetData set_shader_binary, void* user_data)
		{
			GfxNsightAftermathGpuCrashTracker* gpu_crash_tracker = reinterpret_cast<GfxNsightAftermathGpuCrashTracker*>(user_data);
			gpu_crash_tracker->OnShaderLookup(*shader_hash, set_shader_binary);
		}

		// Shader source debug info lookup callback.
		static void ShaderSourceDebugInfoLookupCallback(GFSDK_Aftermath_ShaderDebugName const* shader_debug_name, PFN_GFSDK_Aftermath_SetData set_shader_binary, void* user_data)
		{
			GfxNsightAftermathGpuCrashTracker* gpu_crash_tracker = reinterpret_cast<GfxNsightAftermathGpuCrashTracker*>(user_data);
			gpu_crash_tracker->OnShaderSourceDebugInfoLookup(*shader_debug_name, set_shader_binary);
		}

		void WriteGpuCrashDumpToFile(void const* gpu_crash_dump_data, uint32 gpu_crash_dump_size)
		{
			//TODO
		}

		void WriteShaderDebugInformationToFile(GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier, void const* shader_debug_info, uint32 shader_debug_info_size)
		{
			//TODO
		}
	};
}