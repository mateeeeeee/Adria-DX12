#include "RayTracer.h"

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


	RayTracer::RayTracer(tecs::registry& reg, GraphicsCoreDX12* gfx, u32 width, u32 height) : reg{ reg }, gfx{ gfx }, width{ width }, height{ height }
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

	void RayTracer::BuildBottomLevelAS()
	{
		auto device = gfx->Device();
		auto cmd_list = gfx->DefaultCommandList();
		auto ray_tracing_view = reg.view<Mesh, Transform, RayTracing>();

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
		inputs.NumDescs = geo_descs.size();
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

	void RayTracer::CreateRootSignatures()
	{

	}

	void RayTracer::CreateStateObjects()
	{

	}

}

