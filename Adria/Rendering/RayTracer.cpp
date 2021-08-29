#include "RayTracer.h"
#include "Components.h"
#include "../Graphics/ShaderUtility.h"
#include "../Logging/Logger.h"

#define TO_HANDLE(x) (*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(x))

namespace adria
{
	//helper for making buffers, move it somewhere else later
	static ID3D12Resource* CreateBuffer(ID3D12Device5* device, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, D3D12_HEAP_PROPERTIES const& heap_props)
	{
		D3D12_RESOURCE_DESC buf_desc = {};
		buf_desc.Alignment = 0;
		buf_desc.DepthOrArraySize = 1;
		buf_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buf_desc.Flags = flags;
		buf_desc.Format = DXGI_FORMAT_UNKNOWN;
		buf_desc.Height = 1;
		buf_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buf_desc.MipLevels = 1;
		buf_desc.SampleDesc.Count = 1;
		buf_desc.SampleDesc.Quality = 0;
		buf_desc.Width = size;

		ID3D12Resource* buffer = nullptr;
		BREAK_IF_FAILED(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &buf_desc, init_state, nullptr, IID_PPV_ARGS(&buffer)));
		return buffer;
	}


	RayTracer::RayTracer(tecs::registry& reg, GraphicsCoreDX12* gfx, u32 width, u32 height) 
		: reg{ reg }, gfx{ gfx }, width{ width }, height{ height }
	{
		ID3D12Device* device = gfx->Device();
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
		HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
		if (FAILED(hr) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		{
			Log::Info("Ray Tracing is not supported! All Ray Tracing calls will be silently ignored!\n");
			ray_tracing_supported = false;
		}
		else ray_tracing_supported = true;

		CreateResources();
		CreateRootSignatures();
		CreateStateObjects();
		CreateShaderTables();
	}

	bool RayTracer::IsSupported() const
	{
		return ray_tracing_supported;
	}

	void RayTracer::BuildAccelerationStructures()
	{
		if (!ray_tracing_supported) return;

		BuildBottomLevelAS();
		BuildTopLevelAS();
	}

	Texture2D& RayTracer::RayTraceShadows(Texture2D const& gbuffer_pos)
	{
		return rt_shadows_output;
	}

	void RayTracer::CreateResources()
	{
		ID3D12Device5* device = gfx->Device();

		dxr_heap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 50);

		texture2d_desc_t uav_target_desc{};
		uav_target_desc.width = width;
		uav_target_desc.height = height;
		uav_target_desc.format = DXGI_FORMAT_R8_UNORM;
		uav_target_desc.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		uav_target_desc.start_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		size_t current_handle_index = 0;
		rt_shadows_output = Texture2D(device, uav_target_desc);
		rt_shadows_output.CreateSRV(dxr_heap->GetCpuHandle(current_handle_index++));
		rt_shadows_output.CreateUAV(dxr_heap->GetCpuHandle(current_handle_index++));

		rtao_output = Texture2D(device, uav_target_desc);
		rtao_output.CreateSRV(dxr_heap->GetCpuHandle(current_handle_index++));
		rtao_output.CreateUAV(dxr_heap->GetCpuHandle(current_handle_index++));
	}

	void RayTracer::CreateRootSignatures()
	{
		ID3D12Device5* device = gfx->Device();

		/*global root signature*/
		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		std::array<CD3DX12_ROOT_PARAMETER1, 6> root_parameters{};
		CD3DX12_ROOT_PARAMETER1 root_parameter{};

		//fill root parameters

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
		root_signature_desc.Init_1_1((u32)root_parameters.size(), root_parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
		if (error) OutputDebugStringA((char*)error->GetBufferPointer());

		BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rt_shadows_root_signature)));

	}

	void RayTracer::CreateStateObjects()
	{
		ID3D12Device5* device = gfx->Device();

		ShaderInfo compile_info{};
		compile_info.stage = ShaderStage::LIB;
		compile_info.shadersource = "Resources/Shaders/RayTracing/RayTracedShadows.hlsl";
		ShaderBlob rt_shadows_blob;
		ShaderUtility::CompileShader(compile_info, rt_shadows_blob);

		StateObjectBuilder rt_shadows_state_object_builder(5);

		D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc = {};
		dxil_lib_desc.DXILLibrary.BytecodeLength = rt_shadows_blob.GetLength();
		dxil_lib_desc.DXILLibrary.pShaderBytecode = rt_shadows_blob.GetPointer();
		dxil_lib_desc.NumExports = 0;
		dxil_lib_desc.pExports = nullptr;
		rt_shadows_state_object_builder.AddSubObject(dxil_lib_desc);

		// Add a state subobject for the shader payload configuration
		D3D12_RAYTRACING_SHADER_CONFIG rt_shadows_shader_desc = {};
		rt_shadows_shader_desc.MaxPayloadSizeInBytes = 4;	//bool in hlsl is 4 bytes
		rt_shadows_shader_desc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
		rt_shadows_state_object_builder.AddSubObject(rt_shadows_shader_desc);

		D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
		global_root_sig.pGlobalRootSignature = rt_shadows_root_signature.Get();
		rt_shadows_state_object_builder.AddSubObject(global_root_sig);

		// Add a state subobject for the ray tracing pipeline config
		D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
		pipeline_config.MaxTraceRecursionDepth = 1;
		rt_shadows_state_object_builder.AddSubObject(pipeline_config);

		D3D12_HIT_GROUP_DESC anyhit_group{};
		anyhit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		anyhit_group.AnyHitShaderImport = L"Rts_Anyhit";
		anyhit_group.HitGroupExport = L"ShadowAnyHitGroup";
		rt_shadows_state_object_builder.AddSubObject(anyhit_group);

		rt_shadows_state_object = rt_shadows_state_object_builder.CreateStateObject(device);
	}
	 
	void RayTracer::CreateShaderTables()
	{
		ID3D12Device5* device = gfx->Device();
		
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pso_info = nullptr;
		BREAK_IF_FAILED(rt_shadows_state_object->QueryInterface(IID_PPV_ARGS(&pso_info)));
		
		void const* rts_ray_gen_id = pso_info->GetShaderIdentifier(L"RTS_RayGen");
		void const* rts_anyhit_id = pso_info->GetShaderIdentifier(L"ShadowAnyHitGroup");
		void const* rts_miss_id = pso_info->GetShaderIdentifier(L"RTS_Miss");

		rt_shadows_shader_table_raygen = std::make_unique<ShaderTable>(device, 1);
		rt_shadows_shader_table_raygen->AddShaderRecord(ShaderRecord(rts_ray_gen_id));

		rt_shadows_shader_table_hit = std::make_unique<ShaderTable>(device, 1);
		rt_shadows_shader_table_hit->AddShaderRecord(ShaderRecord(rts_anyhit_id));

		rt_shadows_shader_table_miss = std::make_unique<ShaderTable>(device, 1);
		rt_shadows_shader_table_miss->AddShaderRecord(ShaderRecord(rts_miss_id));
	}

	void RayTracer::BuildBottomLevelAS()
	{
		auto device = gfx->Device();
		auto cmd_list = gfx->DefaultCommandList();
		auto ray_tracing_view = reg.view<Mesh, Transform>();

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geo_descs{};
		for (auto entity : ray_tracing_view)
		{
			auto const& mesh = ray_tracing_view.get<Mesh const>(entity);

			D3D12_RAYTRACING_GEOMETRY_DESC geo_desc{};
			geo_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			geo_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(CompleteVertex);
			geo_desc.Triangles.VertexBuffer.StartAddress = mesh.vb->View().BufferLocation + geo_desc.Triangles.VertexBuffer.StrideInBytes * mesh.vertex_offset;
			geo_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			geo_desc.Triangles.VertexCount = mesh.vertex_count;
			geo_desc.Triangles.IndexFormat = mesh.ib->View().Format;
			geo_desc.Triangles.IndexBuffer = mesh.ib->View().BufferLocation + sizeof(geo_desc.Triangles.IndexFormat) * mesh.start_index_location;
			geo_desc.Triangles.IndexCount = mesh.indices_count;
			geo_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
			geo_descs.push_back(geo_desc);
		}

		// Get required sizes for an acceleration structure
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
		inputs.NumDescs = (u32)geo_descs.size();
		inputs.pGeometryDescs = geo_descs.data();
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

		auto default_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		AccelerationStructureBuffers buffers{};
		buffers.scratch_buffer = CreateBuffer(device, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, default_heap);
		buffers.result_buffer = CreateBuffer(device, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, default_heap);

		// Create the bottom-level AS
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc = {};
		as_desc.Inputs = inputs;
		as_desc.DestAccelerationStructureData = buffers.result_buffer->GetGPUVirtualAddress();
		as_desc.ScratchAccelerationStructureData = buffers.scratch_buffer->GetGPUVirtualAddress();

		cmd_list->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
		D3D12_RESOURCE_BARRIER uav_barrier = {};
		uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uav_barrier.UAV.pResource = buffers.result_buffer.Get();
		cmd_list->ResourceBarrier(1, &uav_barrier);

		blas = buffers.result_buffer;
	}

	void RayTracer::BuildTopLevelAS()
	{

		auto device = gfx->Device();
		auto cmd_list = gfx->DefaultCommandList();

		// First, get the size of the TLAS buffers and create them
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
		inputs.NumDescs = 1;
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

		// Create the buffers
		auto default_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		AccelerationStructureBuffers buffers{};
		buffers.scratch_buffer = CreateBuffer(device, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, default_heap);
		buffers.result_buffer = CreateBuffer(device, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, default_heap);
		tlas_size = info.ResultDataMaxSizeInBytes;

		auto upload_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		buffers.instance_desc_buffer = CreateBuffer(device, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, upload_heap);
		D3D12_RAYTRACING_INSTANCE_DESC* instance_desc = nullptr;
		buffers.instance_desc_buffer->Map(0, nullptr, (void**)&instance_desc);

		// Initialize the instance desc. We only have a single instance

		instance_desc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
		instance_desc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
		instance_desc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instance_desc->Transform[0][0] = instance_desc->Transform[1][1] = instance_desc->Transform[2][2] = 1.0f;
		//DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
		//memcpy(instance_desc->Transform, &identity, sizeof(instance_desc->Transform));
		instance_desc->AccelerationStructure = blas->GetGPUVirtualAddress();
		instance_desc->InstanceMask = 0xFF;

		// Unmap
		buffers.instance_desc_buffer->Unmap(0, nullptr);

		// Create the TLAS
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc = {};
		as_desc.Inputs = inputs;
		as_desc.Inputs.InstanceDescs = buffers.instance_desc_buffer->GetGPUVirtualAddress();
		as_desc.DestAccelerationStructureData = buffers.result_buffer->GetGPUVirtualAddress();
		as_desc.ScratchAccelerationStructureData = buffers.scratch_buffer->GetGPUVirtualAddress();
		cmd_list->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);

		// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
		D3D12_RESOURCE_BARRIER uavBarrier = {};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = buffers.result_buffer.Get();
		cmd_list->ResourceBarrier(1, &uavBarrier);

		tlas = buffers.result_buffer.Get();
	}


}

