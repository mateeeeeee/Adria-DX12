#include "SimpleRayTracer.h"
#include "Components.h"
#include "../Graphics/ShaderUtility.h"
#include "../Logging/Logger.h"
#include "pix3.h"

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

	SimpleRayTracer::SimpleRayTracer(tecs::registry& reg, GraphicsCoreDX12* gfx, uint32 width, uint32 height) 
		: reg{ reg }, gfx{ gfx }, width{ width }, height{ height }, ray_tracing_cbuffer(gfx->GetDevice(), gfx->BackbufferCount())
	{
		ID3D12Device* device = gfx->GetDevice();
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
		HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
		if (FAILED(hr) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		{
			ADRIA_LOG(INFO, "Ray Tracing is not supported! All Ray Tracing calls will be silently ignored!");
			ray_tracing_supported = false;
			return;
		}
		else ray_tracing_supported = true;

		CreateResources();
		CreateRootSignatures();
		CreateStateObjects();
		CreateShaderTables();
	}

	bool SimpleRayTracer::IsSupported() const
	{
		return ray_tracing_supported;
	}

	void SimpleRayTracer::BuildAccelerationStructures()
	{
		if (!ray_tracing_supported) return;

		BuildBottomLevelAS();
		BuildTopLevelAS();
	}

	void SimpleRayTracer::Update(RayTracingParams const& params)
	{
		if (!ray_tracing_supported) return;

		ray_tracing_cbuf_data.frame_count = gfx->FrameIndex();
		ray_tracing_cbuf_data.rtao_radius = params.ao_radius;

		ray_tracing_cbuffer.Update(ray_tracing_cbuf_data, gfx->BackbufferIndex());
	}

	void SimpleRayTracer::RayTraceShadows(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& depth,
		D3D12_GPU_VIRTUAL_ADDRESS frame_cbuf_address,
		D3D12_GPU_VIRTUAL_ADDRESS light_cbuf_address, bool soft_shadows)
	{
		if (!ray_tracing_supported) return;
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "Ray Traced Shadows Pass");

		auto device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		ResourceBarrierBatch rts_barrier{};
		rts_barrier.AddTransition(rt_shadows_output.Resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		rts_barrier.Submit(cmd_list);

		cmd_list->SetComputeRootSignature(rt_shadows_root_signature.Get());

		cmd_list->SetComputeRootConstantBufferView(0, frame_cbuf_address);
		cmd_list->SetComputeRootConstantBufferView(1, light_cbuf_address);
		cmd_list->SetComputeRootConstantBufferView(2, ray_tracing_cbuffer.View(gfx->BackbufferIndex()).BufferLocation);
		cmd_list->SetComputeRootShaderResourceView(3, tlas->GetGPUVirtualAddress());

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(1);
		auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		device->CopyDescriptorsSimple(1, dst_descriptor, depth.SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(descriptor_index));

		descriptor_index = descriptor_allocator->AllocateRange(1);
		dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		device->CopyDescriptorsSimple(1, dst_descriptor, rt_shadows_output.UAV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(5, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->SetPipelineState1(rt_shadows_state_object.Get());

		D3D12_DISPATCH_RAYS_DESC dispatch_desc = {};
		dispatch_desc.HitGroupTable = rt_shadows_shader_table_hit->GetRangeAndStride();
		dispatch_desc.MissShaderTable = rt_shadows_shader_table_miss->GetRangeAndStride();
		dispatch_desc.RayGenerationShaderRecord = rt_shadows_shader_table_raygen->GetRange(static_cast<UINT>(soft_shadows));
		dispatch_desc.Width = width;
		dispatch_desc.Height = height;
		dispatch_desc.Depth = 1;

		cmd_list->DispatchRays(&dispatch_desc);

		rts_barrier.ReverseTransitions();
		rts_barrier.Submit(cmd_list);

	}

	void SimpleRayTracer::RayTraceAmbientOcclusion(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& depth, Texture2D const& normal_gbuf,
		D3D12_GPU_VIRTUAL_ADDRESS frame_cbuf_address)
	{
		if (!ray_tracing_supported) return;
		PIXScopedEvent(cmd_list, PIX_COLOR_DEFAULT, "RTAO Pass");

		auto device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetDescriptorAllocator();

		ResourceBarrierBatch rtao_barrier{};
		rtao_barrier.AddTransition(rtao_output.Resource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		rtao_barrier.Submit(cmd_list);

		cmd_list->SetComputeRootSignature(rtao_root_signature.Get());

		cmd_list->SetComputeRootConstantBufferView(0, frame_cbuf_address);
		cmd_list->SetComputeRootConstantBufferView(1, ray_tracing_cbuffer.View(gfx->BackbufferIndex()).BufferLocation);
		cmd_list->SetComputeRootShaderResourceView(2, tlas->GetGPUVirtualAddress());

		OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
		auto dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		device->CopyDescriptorsSimple(1, dst_descriptor, depth.SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index + 1);
		device->CopyDescriptorsSimple(1, dst_descriptor, normal_gbuf.SRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(3, descriptor_allocator->GetGpuHandle(descriptor_index));

		descriptor_index = descriptor_allocator->AllocateRange(1);
		dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
		device->CopyDescriptorsSimple(1, dst_descriptor, rtao_output.UAV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetComputeRootDescriptorTable(4, descriptor_allocator->GetGpuHandle(descriptor_index));
		cmd_list->SetPipelineState1(rtao_state_object.Get());

		D3D12_DISPATCH_RAYS_DESC dispatch_desc = {};
		dispatch_desc.HitGroupTable = rtao_shader_table_hit->GetRangeAndStride();
		dispatch_desc.MissShaderTable = rtao_shader_table_miss->GetRangeAndStride();
		dispatch_desc.RayGenerationShaderRecord = rtao_shader_table_raygen->GetRange(0);
		dispatch_desc.Width = width;
		dispatch_desc.Height = height;
		dispatch_desc.Depth = 1;

		cmd_list->DispatchRays(&dispatch_desc);

		rtao_barrier.ReverseTransitions();
		rtao_barrier.Submit(cmd_list);
	}

	void SimpleRayTracer::CreateResources()
	{
		ID3D12Device5* device = gfx->GetDevice();

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

		uav_target_desc.start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		rtao_output = Texture2D(device, uav_target_desc);
		rtao_output.CreateSRV(dxr_heap->GetCpuHandle(current_handle_index++));
		rtao_output.CreateUAV(dxr_heap->GetCpuHandle(current_handle_index++));
	}

	void SimpleRayTracer::CreateRootSignatures()
	{
		ID3D12Device5* device = gfx->GetDevice();

		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		//ray traced shadows
		{
			std::array<CD3DX12_ROOT_PARAMETER1, 6> root_parameters{};
			root_parameters[0].InitAsConstantBufferView(0);
			root_parameters[1].InitAsConstantBufferView(2);
			root_parameters[2].InitAsConstantBufferView(10);
			root_parameters[3].InitAsShaderResourceView(0);

			D3D12_DESCRIPTOR_RANGE1 srv_range = {};
			srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			srv_range.NumDescriptors = 1;
			srv_range.BaseShaderRegister = 1;
			srv_range.RegisterSpace = 0;
			srv_range.OffsetInDescriptorsFromTableStart = 0;
			root_parameters[4].InitAsDescriptorTable(1, &srv_range);

			D3D12_DESCRIPTOR_RANGE1 uav_range = {};
			uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			uav_range.NumDescriptors = 1;
			uav_range.BaseShaderRegister = 0;
			uav_range.RegisterSpace = 0;
			uav_range.OffsetInDescriptorsFromTableStart = 0;
			root_parameters[5].InitAsDescriptorTable(1, &uav_range);
			//fill root parameters

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
			root_signature_desc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 0, nullptr);

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
			if (error)
			{
				ADRIA_LOG(ERROR, (char*)error->GetBufferPointer());
				ADRIA_ASSERT(FALSE);
			}
			BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rt_shadows_root_signature)));
		}

		//rtao
		{
			std::array<CD3DX12_ROOT_PARAMETER1, 5> root_parameters{};
			root_parameters[0].InitAsConstantBufferView(0);
			root_parameters[1].InitAsConstantBufferView(10);
			root_parameters[2].InitAsShaderResourceView(0);

			D3D12_DESCRIPTOR_RANGE1 srv_range = {};
			srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			srv_range.NumDescriptors = 2;
			srv_range.BaseShaderRegister = 1;
			srv_range.RegisterSpace = 0;
			srv_range.OffsetInDescriptorsFromTableStart = 0;
			root_parameters[3].InitAsDescriptorTable(1, &srv_range);

			D3D12_DESCRIPTOR_RANGE1 uav_range = {};
			uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			uav_range.NumDescriptors = 1;
			uav_range.BaseShaderRegister = 0;
			uav_range.RegisterSpace = 0;
			uav_range.OffsetInDescriptorsFromTableStart = 0;
			root_parameters[4].InitAsDescriptorTable(1, &uav_range);
			//fill root parameters

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
			root_signature_desc.Init_1_1((uint32)root_parameters.size(), root_parameters.data(), 0, nullptr);

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &signature, &error);
			if (error)
			{
				ADRIA_LOG(ERROR, (char*)error->GetBufferPointer());
				ADRIA_ASSERT(FALSE);
			}
			BREAK_IF_FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rtao_root_signature)));
		}
	}

	void SimpleRayTracer::CreateStateObjects()
	{
		ID3D12Device5* device = gfx->GetDevice();

		ShaderInfo compile_info{};
		compile_info.stage = EShaderStage::LIB;
		compile_info.shadersource = "Resources/Shaders/RayTracing/RayTracedShadows.hlsl";
		ShaderBlob rt_shadows_blob;
		ShaderUtility::CompileShader(compile_info, rt_shadows_blob);

		compile_info.macros.emplace_back(L"SOFT_SHADOWS", L"");
		ShaderBlob rt_soft_shadows_blob;
		ShaderUtility::CompileShader(compile_info, rt_soft_shadows_blob);
		compile_info.macros.clear();

		compile_info.shadersource = "Resources/Shaders/RayTracing/RayTracedAmbientOcclusion.hlsl";
		ShaderBlob rtao_blob;
		ShaderUtility::CompileShader(compile_info, rtao_blob);

		StateObjectBuilder rt_shadows_state_object_builder(6);
		{
			D3D12_EXPORT_DESC export_descs[] = 
			{
				D3D12_EXPORT_DESC{.Name = L"RTS_RayGen_Hard", .ExportToRename = L"RTS_RayGen"},
				D3D12_EXPORT_DESC{.Name = L"RTS_Anyhit", .ExportToRename = NULL},
				D3D12_EXPORT_DESC{.Name = L"RTS_Miss", .ExportToRename = NULL}
			};

			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc = {};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rt_shadows_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rt_shadows_blob.GetPointer();
			dxil_lib_desc.NumExports = ARRAYSIZE(export_descs);
			dxil_lib_desc.pExports = export_descs;
			rt_shadows_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc2 = {};
			D3D12_EXPORT_DESC export_desc2{};
			export_desc2.ExportToRename = L"RTS_RayGen";
			export_desc2.Name = L"RTS_RayGen_Soft";
			dxil_lib_desc2.DXILLibrary.BytecodeLength = rt_soft_shadows_blob.GetLength();
			dxil_lib_desc2.DXILLibrary.pShaderBytecode = rt_soft_shadows_blob.GetPointer();
			dxil_lib_desc2.NumExports = 1;
			dxil_lib_desc2.pExports = &export_desc2;
			rt_shadows_state_object_builder.AddSubObject(dxil_lib_desc2);

			// Add a state subobject for the shader payload configuration
			D3D12_RAYTRACING_SHADER_CONFIG rt_shadows_shader_config = {};
			rt_shadows_shader_config.MaxPayloadSizeInBytes = 4;	//bool in hlsl is 4 bytes
			rt_shadows_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			rt_shadows_state_object_builder.AddSubObject(rt_shadows_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = rt_shadows_root_signature.Get();
			rt_shadows_state_object_builder.AddSubObject(global_root_sig);

			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
			pipeline_config.MaxTraceRecursionDepth = 1;
			rt_shadows_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC anyhit_group{};
			anyhit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			anyhit_group.AnyHitShaderImport = L"RTS_Anyhit";
			anyhit_group.HitGroupExport = L"ShadowAnyHitGroup";
			rt_shadows_state_object_builder.AddSubObject(anyhit_group);

			rt_shadows_state_object = rt_shadows_state_object_builder.CreateStateObject(device);
		}

		StateObjectBuilder rtao_state_object_builder(5);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc = {};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rtao_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rtao_blob.GetPointer();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			rtao_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG rtao_shader_config = {};
			rtao_shader_config.MaxPayloadSizeInBytes = 4;	//bool in hlsl is 4 bytes
			rtao_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			rtao_state_object_builder.AddSubObject(rtao_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = rtao_root_signature.Get();
			rtao_state_object_builder.AddSubObject(global_root_sig);

			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
			pipeline_config.MaxTraceRecursionDepth = 1;
			rtao_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC anyhit_group{};
			anyhit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			anyhit_group.AnyHitShaderImport = L"RTAO_AnyHit";
			anyhit_group.HitGroupExport = L"RTAOAnyHitGroup";
			rtao_state_object_builder.AddSubObject(anyhit_group);

			rtao_state_object = rtao_state_object_builder.CreateStateObject(device);
		}
		//maybe merge state objects and table together?
	}
	 
	void SimpleRayTracer::CreateShaderTables()
	{
		ID3D12Device5* device = gfx->GetDevice();
		//RTS
		{
			Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pso_info = nullptr;
			BREAK_IF_FAILED(rt_shadows_state_object->QueryInterface(IID_PPV_ARGS(&pso_info)));

			void const* rts_ray_gen_hard_id = pso_info->GetShaderIdentifier(L"RTS_RayGen_Hard");
			void const* rts_ray_gen_soft_id = pso_info->GetShaderIdentifier(L"RTS_RayGen_Soft");
			void const* rts_anyhit_id = pso_info->GetShaderIdentifier(L"ShadowAnyHitGroup");
			void const* rts_miss_id = pso_info->GetShaderIdentifier(L"RTS_Miss");

			rt_shadows_shader_table_raygen = std::make_unique<ShaderTable>(device, 2);
			rt_shadows_shader_table_raygen->AddShaderRecord(ShaderRecord(rts_ray_gen_hard_id));
			rt_shadows_shader_table_raygen->AddShaderRecord(ShaderRecord(rts_ray_gen_soft_id));

			rt_shadows_shader_table_hit = std::make_unique<ShaderTable>(device, 1);
			rt_shadows_shader_table_hit->AddShaderRecord(ShaderRecord(rts_anyhit_id));

			rt_shadows_shader_table_miss = std::make_unique<ShaderTable>(device, 1);
			rt_shadows_shader_table_miss->AddShaderRecord(ShaderRecord(rts_miss_id));
		}
		//RTAO
		{
			Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pso_info = nullptr;
			BREAK_IF_FAILED(rtao_state_object->QueryInterface(IID_PPV_ARGS(&pso_info)));

			void const* rtao_ray_gen_id = pso_info->GetShaderIdentifier(L"RTAO_RayGen");
			void const* rtao_anyhit_id = pso_info->GetShaderIdentifier(L"RTAOAnyHitGroup");
			void const* rtao_miss_id = pso_info->GetShaderIdentifier(L"RTAO_Miss");

			rtao_shader_table_raygen = std::make_unique<ShaderTable>(device, 1);
			rtao_shader_table_raygen->AddShaderRecord(ShaderRecord(rtao_ray_gen_id));

			rtao_shader_table_hit = std::make_unique<ShaderTable>(device, 1);
			rtao_shader_table_hit->AddShaderRecord(ShaderRecord(rtao_anyhit_id));

			rtao_shader_table_miss = std::make_unique<ShaderTable>(device, 1);
			rtao_shader_table_miss->AddShaderRecord(ShaderRecord(rtao_miss_id));
		}
	}

	void SimpleRayTracer::BuildBottomLevelAS()
	{
		auto device = gfx->GetDevice();
		auto cmd_list = gfx->GetDefaultCommandList();
		auto upload_allocator = gfx->GetUploadBuffer();
		auto ray_tracing_view = reg.view<Mesh, Transform, RayTracing>();

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geo_descs{};
		for (auto entity : ray_tracing_view)
		{
			auto const& mesh = ray_tracing_view.get<Mesh const>(entity);

			auto const& transform = ray_tracing_view.get<Transform const>(entity);
			DynamicAllocation transform_alloc = upload_allocator->Allocate(sizeof(DirectX::XMMATRIX), D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT);
			transform_alloc.Update(transform.current_transform);
			
			D3D12_RAYTRACING_GEOMETRY_DESC geo_desc{};
			geo_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			geo_desc.Triangles.Transform3x4 = transform_alloc.gpu_address;
			geo_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(CompleteVertex);
			geo_desc.Triangles.VertexBuffer.StartAddress = mesh.vertex_buffer->View().BufferLocation + geo_desc.Triangles.VertexBuffer.StrideInBytes * mesh.base_vertex_location;
			geo_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			geo_desc.Triangles.VertexCount = mesh.vertex_count;
			geo_desc.Triangles.IndexFormat = mesh.index_buffer->View().Format;
			geo_desc.Triangles.IndexBuffer = mesh.index_buffer->View().BufferLocation + mesh.start_index_location * (geo_desc.Triangles.IndexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4);
			geo_desc.Triangles.IndexCount = mesh.indices_count;
			geo_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
			geo_descs.push_back(geo_desc);
		}

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bl_prebuild_info{};
		
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.NumDescs = static_cast<uint32>(geo_descs.size());
		inputs.pGeometryDescs = geo_descs.data();
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &bl_prebuild_info);
		
		ADRIA_ASSERT(bl_prebuild_info.ResultDataMaxSizeInBytes > 0);

		auto default_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		struct BLAccelerationStructureBuffers
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> scratch_buffer;
			Microsoft::WRL::ComPtr<ID3D12Resource> result_buffer;
		} blas_buffers{};

		blas_buffers.scratch_buffer = CreateBuffer(device, bl_prebuild_info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, default_heap);
		blas_buffers.result_buffer = CreateBuffer(device, bl_prebuild_info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, default_heap);

		// Create the bottom-level AS
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blas_desc{};
		blas_desc.Inputs = inputs;
		blas_desc.DestAccelerationStructureData = blas_buffers.result_buffer->GetGPUVirtualAddress();
		blas_desc.ScratchAccelerationStructureData = blas_buffers.scratch_buffer->GetGPUVirtualAddress();

		cmd_list->BuildRaytracingAccelerationStructure(&blas_desc, 0, nullptr);

		D3D12_RESOURCE_BARRIER uav_barrier{};
		uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uav_barrier.UAV.pResource = blas_buffers.result_buffer.Get();
		cmd_list->ResourceBarrier(1, &uav_barrier);

		blas = blas_buffers.result_buffer;
	}

	void SimpleRayTracer::BuildTopLevelAS()
	{
		auto device = gfx->GetDevice();
		auto cmd_list = gfx->GetDefaultCommandList();

		struct TLAccelerationStructureBuffers
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> instance_buffer;
			Microsoft::WRL::ComPtr<ID3D12Resource> scratch_buffer;
			Microsoft::WRL::ComPtr<ID3D12Resource> result_buffer;
		} tlas_buffers{};

		// First, get the size of the TLAS buffers and create them
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		inputs.NumDescs = 1;
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tl_prebuild_info;
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &tl_prebuild_info);
		ADRIA_ASSERT(tl_prebuild_info.ResultDataMaxSizeInBytes > 0);

		auto default_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		tlas_buffers.scratch_buffer = CreateBuffer(device, tl_prebuild_info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, default_heap);
		tlas_buffers.result_buffer  = CreateBuffer(device, tl_prebuild_info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, default_heap);

		auto upload_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		tlas_buffers.instance_buffer = CreateBuffer(device, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, upload_heap);
		D3D12_RAYTRACING_INSTANCE_DESC* p_instance_desc = nullptr;
		tlas_buffers.instance_buffer->Map(0, nullptr, (void**)&p_instance_desc);

		p_instance_desc->InstanceID = 0;                            
		p_instance_desc->InstanceContributionToHitGroupIndex = 0;   
		p_instance_desc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		static const auto I = DirectX::XMMatrixIdentity();
		memcpy(p_instance_desc->Transform, &I, sizeof(p_instance_desc->Transform));
		p_instance_desc->AccelerationStructure = blas->GetGPUVirtualAddress();
		p_instance_desc->InstanceMask = 0xFF;
		tlas_buffers.instance_buffer->Unmap(0, nullptr);

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlas_desc = {};
		tlas_desc.Inputs = inputs;
		tlas_desc.Inputs.InstanceDescs = tlas_buffers.instance_buffer->GetGPUVirtualAddress();
		tlas_desc.DestAccelerationStructureData = tlas_buffers.result_buffer->GetGPUVirtualAddress();
		tlas_desc.ScratchAccelerationStructureData = tlas_buffers.scratch_buffer->GetGPUVirtualAddress();
		cmd_list->BuildRaytracingAccelerationStructure(&tlas_desc, 0, nullptr);
		
		D3D12_RESOURCE_BARRIER uav_barrier{};
		uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uav_barrier.UAV.pResource = tlas_buffers.result_buffer.Get();
		cmd_list->ResourceBarrier(1, &uav_barrier);

		tlas = tlas_buffers.result_buffer;
	}

}
