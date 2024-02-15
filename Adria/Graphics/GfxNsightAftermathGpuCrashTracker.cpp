#include <iomanip>
#include "GfxNsightAftermathGpuCrashTracker.h"
#include "GFSDK_Aftermath.h"
#include "GfxShader.h"
#include "GfxDevice.h"
#include "Rendering/ShaderCache.h"
#include "Logging/Logger.h"

namespace adria
{
	//	void AddToShaderHashMap(GfxShaderID shader_id)
	//	{
	//#if GFX_NSIGHT_AFTERMATH
	//		GfxShader const& shader = shader_map[shader_id];
	//		D3D12_SHADER_BYTECODE shader_bytecode{ shader.GetData(), shader.GetLength() };
	//		GFSDK_Aftermath_ShaderBinaryHash shader_hash;
	//		bool result = GFSDK_Aftermath_GetShaderHash(GFSDK_Aftermath_Version_API, &shader_bytecode, &shader_hash);
	//		ADRIA_ASSERT(result == GFSDK_Aftermath_Result_Success);
	//		shader_hash_map[shader_hash.hash] = shader_id;
	//#endif
	//	}


	template<typename T>
	std::string GfxNsightAftermathGpuCrashTracker::ToHexString(T n)
	{
		std::stringstream stream;
		stream << std::setfill('0') << std::setw(2 * sizeof(T)) << std::hex << n;
		return stream.str();
	}

	GfxNsightAftermathGpuCrashTracker::GfxNsightAftermathGpuCrashTracker(GfxDevice* gfx)
	{
		if (gfx->GetVendor() != GfxVendor::Nvidia)
		{
			ADRIA_LOG(WARNING, "Cannot use Nvidia Aftermath because the GPU vendor is not Nvidia!");
			return;
		}
		uint32 const aftermath_flags =
			GFSDK_Aftermath_FeatureFlags_EnableMarkers |             // Enable event marker tracking.
			GFSDK_Aftermath_FeatureFlags_CallStackCapturing |        // Enable automatic call stack event markers.
			GFSDK_Aftermath_FeatureFlags_EnableResourceTracking |    // Enable tracking of resources.
			GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo |   // Generate debug information for shaders.
			GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting; // Enable additional runtime shader error reporting.

		GFSDK_Aftermath_Result result = GFSDK_Aftermath_DX12_Initialize(GFSDK_Aftermath_Version_API, aftermath_flags, gfx->GetDevice());
		if (result != GFSDK_Aftermath_Result_Success)
		{
			ADRIA_LOG(WARNING, "GFSDK_Aftermath_DX12_Initialize call failed!");
			initialized = false;
			return;
		}

		result = GFSDK_Aftermath_EnableGpuCrashDumps(
			GFSDK_Aftermath_Version_API,
			GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX,
			GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,	// Let the Nsight Aftermath library cache shader debug information.
			GpuCrashDumpCallback,												// Register callback for GPU crash dumps.
			ShaderDebugInfoCallback,											// Register callback for shader debug information.
			CrashDumpDescriptionCallback,										// Register callback for GPU crash dump description.
			NULL,																// Register callback for resolving application-managed markers.
			this);																// Set the GpuCrashTracker object as user data for the above callbacks.
		initialized = (result == GFSDK_Aftermath_Result_Success);
		ShaderCache::GetShaderRecompiledEvent().AddMember(&GfxNsightAftermathGpuCrashTracker::OnShaderOrLibraryCompiled, *this);
	}

	GfxNsightAftermathGpuCrashTracker::~GfxNsightAftermathGpuCrashTracker()
	{
		if (initialized) GFSDK_Aftermath_DisableGpuCrashDumps();
	}

	void GfxNsightAftermathGpuCrashTracker::HandleGpuHang()
	{
		GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
		GFSDK_Aftermath_GetCrashDumpStatus(&status);
		while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
			status != GFSDK_Aftermath_CrashDump_Status_Finished)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			GFSDK_Aftermath_GetCrashDumpStatus(&status);
		}
	}

	void GfxNsightAftermathGpuCrashTracker::OnCrashDump(const void* gpu_crash_dump_data, const uint32 gpu_crash_dump_size)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		WriteGpuCrashDumpToFile(gpu_crash_dump_data, gpu_crash_dump_size);
	}

	void GfxNsightAftermathGpuCrashTracker::OnShaderDebugInfo(const void* shader_debug_info, const uint32 shader_debug_info_size)
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
		shader_debug_info_map[identifier] = std::move(data);
		// Write to file for later in-depth analysis of crash dumps with Nsight Graphics
		WriteShaderDebugInformationToFile(identifier, shader_debug_info, shader_debug_info_size);
	}

	void GfxNsightAftermathGpuCrashTracker::OnDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description)
	{
		add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "Adria-DX12");
		add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "v1.0");
		add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined, "Nvidia Nsight Aftermath crash dump");
	}

	void GfxNsightAftermathGpuCrashTracker::OnShaderDebugInfoLookup(GFSDK_Aftermath_ShaderDebugInfoIdentifier const& identifier, PFN_GFSDK_Aftermath_SetData set_shader_debug_info) const
	{
		if (!shader_debug_info_map.contains(identifier)) return;
		auto const& debug_info = (*shader_debug_info_map.find(identifier)).second;
		// Let the GPU crash dump decoder know about the shader debug information that was found.
		set_shader_debug_info(debug_info.data(), (uint32)debug_info.size());
	}

	// Handler for shader lookup callbacks.
	// This is used by the JSON decoder for mapping shader instruction
	// addresses to DXIL lines or HLSL source lines.
	// NOTE: If the application loads stripped shader binaries (-Qstrip_debug),
	// Aftermath will require access to both the stripped and the not stripped
	// shader binaries.
	void GfxNsightAftermathGpuCrashTracker::OnShaderLookup(GFSDK_Aftermath_ShaderBinaryHash const& shader_hash, PFN_GFSDK_Aftermath_SetData set_shader_binary) const
	{
		GfxShader const& shader = ShaderCache::GetShader(shader_hash_map[shader_hash.hash]);
		set_shader_binary(shader.GetData(), (uint32)shader.GetLength());
	}
	// Handler for shader source debug info lookup callbacks.
	// This is used by the JSON decoder for mapping shader instruction addresses to
	// HLSL source lines, if the shaders used by the application were compiled with
	// separate debug info data files.
	void GfxNsightAftermathGpuCrashTracker::OnShaderSourceDebugInfoLookup(GFSDK_Aftermath_ShaderDebugName const& shaderDebugName, PFN_GFSDK_Aftermath_SetData setShaderBinary) const
	{

	}

	void GfxNsightAftermathGpuCrashTracker::WriteGpuCrashDumpToFile(void const* gpu_crash_dump_data, uint32 gpu_crash_dump_size)
	{
		// Create a GPU crash dump decoder object for the GPU crash dump.
		GFSDK_Aftermath_GpuCrashDump_Decoder decoder{};
		GFSDK_Aftermath_Result result = GFSDK_Aftermath_GpuCrashDump_CreateDecoder(
			GFSDK_Aftermath_Version_API,
			gpu_crash_dump_data,
			gpu_crash_dump_size,
			&decoder);
		if (result != GFSDK_Aftermath_Result_Success) return;

		// Use the decoder object to read basic information, like application
		// name, PID, etc. from the GPU crash dump.
		GFSDK_Aftermath_GpuCrashDump_BaseInfo baseInfo{};
		result = GFSDK_Aftermath_GpuCrashDump_GetBaseInfo(decoder, &baseInfo);
		if (result != GFSDK_Aftermath_Result_Success) return;
		// Use the decoder object to query the application name that was set
		// in the GPU crash dump description.
		uint32 application_name_length = 0;
		GFSDK_Aftermath_GpuCrashDump_GetDescriptionSize(
			decoder, GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
			&application_name_length);

		std::vector<char> application_name(application_name_length, '\0');

		GFSDK_Aftermath_GpuCrashDump_GetDescription(
			decoder, GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName,
			uint32(application_name.size()), application_name.data());

		// Create a unique file name for writing the crash dump data to a file.
		// Note: due to an Nsight Aftermath bug (will be fixed in an upcoming
		// driver release) we may see redundant crash dumps. As a workaround,
		// attach a unique count to each generated file name.
		static int count = 0;
		const std::string base_file_name =
			std::string(application_name.data())
			+ "-"
			+ std::to_string(baseInfo.pid)
			+ "-"
			+ std::to_string(++count);

		// Write the crash dump data to a file using the .nv-gpudmp extension
		// registered with Nsight Graphics.
		std::string const  crash_dump_filename = base_file_name + ".nv-gpudmp";
		std::ofstream dumpFile(crash_dump_filename, std::ios::out | std::ios::binary);
		if (dumpFile)
		{
			dumpFile.write((const char*)gpu_crash_dump_data, gpu_crash_dump_size);
			dumpFile.close();
		}

		// Decode the crash dump to a JSON string.
		// Step 1: Generate the JSON and get the size.
		uint32_t json_size = 0;
		result = GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
			decoder,
			GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
			GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE,
			ShaderDebugInfoLookupCallback,
			ShaderLookupCallback,
			ShaderSourceDebugInfoLookupCallback,
			this,
			&json_size);

		// Step 2: Allocate a buffer and fetch the generated JSON.
		std::vector<char> json(json_size);
		result = GFSDK_Aftermath_GpuCrashDump_GetJSON(decoder, uint32_t(json.size()), json.data());
		if (result != GFSDK_Aftermath_Result_Success) return;

		// Write the crash dump data as JSON to a file.
		const std::string json_filename = crash_dump_filename + ".json";
		std::ofstream json_file(json_filename, std::ios::out | std::ios::binary);
		if (json_file)
		{
			json_file.write(json.data(), json.size() - 1);
			json_file.close();
		}
		GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder);
	}

	void GfxNsightAftermathGpuCrashTracker::WriteShaderDebugInformationToFile(GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier, void const* shader_debug_info, uint32 shader_debug_info_size)
	{
		std::string file_path = "shader-" + ToString(identifier) + ".nvdbg";
		std::ofstream f(file_path, std::ios::out | std::ios::binary);
		if (f) f.write((const char*)shader_debug_info, shader_debug_info_size);
	}

	void GfxNsightAftermathGpuCrashTracker::OnShaderOrLibraryCompiled(GfxShaderID shader_id)
	{
		GfxShader const& shader = ShaderCache::GetShader(shader_id);
		D3D12_SHADER_BYTECODE shader_bytecode{ shader.GetData(), shader.GetLength() };
		GFSDK_Aftermath_ShaderBinaryHash shader_hash;
		bool result = GFSDK_Aftermath_GetShaderHash(GFSDK_Aftermath_Version_API, &shader_bytecode, &shader_hash);
		ADRIA_ASSERT(result == GFSDK_Aftermath_Result_Success);
		shader_hash_map[shader_hash.hash] = shader_id;
	}
}

