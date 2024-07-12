#pragma once
#include <mutex>
#include "Graphics/GfxShaderKey.h"
#include "GFSDK_Aftermath_Defines.h"
#include "GFSDK_Aftermath_GpuCrashDump.h"
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"

namespace adria
{
	struct GFSDK_Aftermath_ShaderDebugInfoIdentifierComparator
	{
		bool operator()(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& lhs, const GFSDK_Aftermath_ShaderDebugInfoIdentifier& rhs) const
		{
			if (lhs.id[0] == rhs.id[0])
			{
				return lhs.id[1] < rhs.id[1];
			}
			return lhs.id[0] < rhs.id[0];
		}
	};

	class GfxDevice;
	enum ShaderID : uint8;

	class GfxNsightAftermathGpuCrashTracker
	{
	public:
		explicit GfxNsightAftermathGpuCrashTracker(GfxDevice* gfx);
		~GfxNsightAftermathGpuCrashTracker();

		void Initialize();
		bool IsInitialized() const { return initialized; }
		void HandleGpuCrash();

	private:
		GfxDevice* gfx;
		bool initialized = false;
		mutable std::mutex m_mutex;
		std::map<GFSDK_Aftermath_ShaderDebugInfoIdentifier, std::vector<uint8>, GFSDK_Aftermath_ShaderDebugInfoIdentifierComparator> shader_debug_info_map;
		mutable std::unordered_map<uint64, GfxShaderKey> shader_hash_map;

	private:
		void OnCrashDump(const void* gpu_crash_dump_data, const uint32 gpu_crash_dump_size);
		void OnShaderDebugInfo(const void* shader_debug_info, const uint32 shader_debug_info_size);
		void OnDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description);
		void OnResolveMarker(void const* marker_data, uint32 marker_data_size, void** resolved_marker_data, uint32* resolved_marker_data_size)
		{

		}
		void OnShaderDebugInfoLookup(GFSDK_Aftermath_ShaderDebugInfoIdentifier const& identifier, PFN_GFSDK_Aftermath_SetData set_shader_debug_info) const;
		
		void OnShaderLookup(GFSDK_Aftermath_ShaderBinaryHash const& shader_hash, PFN_GFSDK_Aftermath_SetData set_shader_binary) const;
		void OnShaderSourceDebugInfoLookup(GFSDK_Aftermath_ShaderDebugName const& shaderDebugName, PFN_GFSDK_Aftermath_SetData setShaderBinary) const;

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
		static void ResolveMarkerCallback(void const* marker_data, uint32 marker_data_size, void* user_data, void** resolved_marker_data, uint32* resolved_marker_data_size)
		{
			GfxNsightAftermathGpuCrashTracker* gpu_crash_tracker = reinterpret_cast<GfxNsightAftermathGpuCrashTracker*>(user_data);
			gpu_crash_tracker->OnResolveMarker(marker_data, marker_data_size, resolved_marker_data, resolved_marker_data_size);
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
		static void ShaderSourceDebugInfoLookupCallback(GFSDK_Aftermath_ShaderDebugName const* shader_debug_name, PFN_GFSDK_Aftermath_SetData set_shader_binary, void* user_data)
		{
			GfxNsightAftermathGpuCrashTracker* gpu_crash_tracker = reinterpret_cast<GfxNsightAftermathGpuCrashTracker*>(user_data);
			gpu_crash_tracker->OnShaderSourceDebugInfoLookup(*shader_debug_name, set_shader_binary);
		}

		void WriteGpuCrashDumpToFile(void const* gpu_crash_dump_data, uint32 gpu_crash_dump_size);
		void WriteShaderDebugInformationToFile(GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier, void const* shader_debug_info, uint32 shader_debug_info_size);

		void OnShaderOrLibraryCompiled(GfxShaderKey const&);
	};

}