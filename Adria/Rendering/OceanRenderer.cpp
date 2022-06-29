#include "OceanRenderer.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "PSOCache.h" 
#include "RootSignatureCache.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/Texture.h"
#include "../Graphics/TextureManager.h"
#include "../tecs/registry.h"
#include "../Utilities/Random.h"
#include "../Math/Constants.h"

using namespace DirectX;

namespace adria
{

	OceanRenderer::OceanRenderer(tecs::registry& reg, TextureManager& texture_manager, uint32 w, uint32 h)
		: reg{ reg }, texture_manager{ texture_manager }, width{ w }, height{ h }
	{}

	void OceanRenderer::UpdateOceanColor(float32(&color)[3])
	{
		if (reg.size<Ocean>() == 0) return;

		auto ocean_view = reg.view<Ocean, Material>();
		for (auto e : ocean_view)
		{
			auto& material = ocean_view.get<Material>(e);
			material.diffuse = XMFLOAT3(color);
		}
	}

	void OceanRenderer::AddPasses(RenderGraph& rendergraph, bool recreate_spectrum, bool tesselated, bool wireframe)
	{
		if (reg.size<Ocean>() == 0) return;
		GlobalBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<GlobalBlackboardData>();

		rendergraph.ImportTexture(RG_RES_NAME(InitialSpectrum), initial_spectrum.get());
		rendergraph.ImportTexture(RG_RES_NAME(PongPhase), ping_pong_phase_textures[pong_phase].get());
		rendergraph.ImportTexture(RG_RES_NAME(PingPhase), ping_pong_phase_textures[!pong_phase].get());
		rendergraph.ImportTexture(RG_RES_NAME(PongSpectrum), ping_pong_spectrum_textures[pong_spectrum].get());
		rendergraph.ImportTexture(RG_RES_NAME(PingSpectrum), ping_pong_spectrum_textures[!pong_spectrum].get());

		if (recreate_spectrum)
		{
			struct InitialSpectrumPassData
			{
				RGTextureReadWriteId initial_spectrum_uav;
			};
			rendergraph.AddPass<InitialSpectrumPassData>("Spectrum Creation Pass",
				[=](InitialSpectrumPassData& data, RenderGraphBuilder& builder)
				{
					data.initial_spectrum_uav = builder.WriteTexture(RG_RES_NAME(InitialSpectrum));
				},
				[=](InitialSpectrumPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
				{
					auto device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

					cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::InitialSpectrum));
					cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::InitialSpectrum));
					cmd_list->SetComputeRootConstantBufferView(0, global_data.compute_cbuffer_address);

					OffsetType descriptor_index = descriptor_allocator->Allocate();
					D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
					device->CopyDescriptorsSimple(1, dst_descriptor, context.GetReadWriteTexture(data.initial_spectrum_uav),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));
					cmd_list->Dispatch(FFT_RESOLUTION / 32, FFT_RESOLUTION / 32, 1);
				}, ERGPassType::Compute, ERGPassFlags::None);
		}
		
		struct PhasePassData
		{
			RGTextureReadOnlyId phase_srv;
			RGTextureReadWriteId phase_uav;
		};
		rendergraph.AddPass<PhasePassData>("Phase Pass",
			[=](PhasePassData& data, RenderGraphBuilder& builder)
			{
				data.phase_srv = builder.ReadTexture(RG_RES_NAME(PongPhase), ReadAccess_NonPixelShader);
				data.phase_uav = builder.WriteTexture(RG_RES_NAME(PingPhase));
			},
			[=](PhasePassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Phase));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Phase));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.compute_cbuffer_address);
				OffsetType descriptor_index = descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
					context.GetReadOnlyTexture(data.phase_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				descriptor_index = descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
					context.GetReadWriteTexture(data.phase_uav),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->Dispatch(FFT_RESOLUTION / 32, FFT_RESOLUTION / 32, 1);
			}, ERGPassType::Compute, ERGPassFlags::None);
		pong_phase = !pong_phase;

		struct SpectrumPassData
		{
			RGTextureReadOnlyId phase_srv;
			RGTextureReadOnlyId initial_spectrum_srv;
			RGTextureReadWriteId spectrum_uav;
		};
		rendergraph.AddPass<SpectrumPassData>("Spectrum Pass",
			[=](SpectrumPassData& data, RenderGraphBuilder& builder)
			{
				data.phase_srv = builder.ReadTexture(RG_RES_NAME(PongPhase), ReadAccess_NonPixelShader);
				data.initial_spectrum_srv = builder.ReadTexture(RG_RES_NAME(InitialSpectrum), ReadAccess_NonPixelShader);
				data.spectrum_uav = builder.WriteTexture(RG_RES_NAME(PongSpectrum));
			},
			[=](SpectrumPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Spectrum));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::Spectrum));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.compute_cbuffer_address);

				D3D12_CPU_DESCRIPTOR_HANDLE srvs[] = { context.GetReadOnlyTexture(data.phase_srv), context.GetReadOnlyTexture(data.initial_spectrum_srv)};
				OffsetType descriptor_index = descriptor_allocator->AllocateRange(ARRAYSIZE(srvs));
				auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
				uint32 dst_range_sizes[] = { ARRAYSIZE(srvs) };
				uint32 src_range_sizes[] = { 1, 1 };
				device->CopyDescriptors(ARRAYSIZE(dst_range_sizes), dst_descriptor.GetCPUAddress(), dst_range_sizes, ARRAYSIZE(src_range_sizes), srvs, src_range_sizes,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				descriptor_index = descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
					context.GetReadWriteTexture(data.spectrum_uav),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));

				cmd_list->Dispatch(FFT_RESOLUTION / 32, FFT_RESOLUTION / 32, 1);

			}, ERGPassType::Compute, ERGPassFlags::None);

		DECLSPEC_ALIGN(16) struct FFTCBuffer
		{
			uint32 seq_count;
			uint32 subseq_count;
		};

		//Add Horizontal FFT
		for (uint32 p = 1; p < FFT_RESOLUTION; p <<= 1)
		{
			struct FFTHorizontalPassData
			{
				RGTextureReadOnlyId spectrum_srv;
				RGTextureReadWriteId spectrum_uav;
			};

			rendergraph.AddPass<FFTHorizontalPassData>("FFT Horizontal Pass",
				[=, pong_spectrum = this->pong_spectrum](FFTHorizontalPassData& data, RenderGraphBuilder& builder)
				{
					RGResourceName pong_spectrum_texture = !pong_spectrum ? RG_RES_NAME(PongSpectrum) : RG_RES_NAME(PingSpectrum);
					RGResourceName ping_spectrum_texture = !pong_spectrum ? RG_RES_NAME(PingSpectrum) : RG_RES_NAME(PongSpectrum);
					data.spectrum_srv = builder.ReadTexture(pong_spectrum_texture, ReadAccess_NonPixelShader);
					data.spectrum_uav = builder.WriteTexture(ping_spectrum_texture);
				},
				[=](FFTHorizontalPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
				{
					auto device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
					auto dynamic_allocator = gfx->GetDynamicAllocator();

					cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::FFT));
					cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::FFT_Horizontal));

					FFTCBuffer fft_cbuf_data{ .seq_count = FFT_RESOLUTION };
					fft_cbuf_data.subseq_count = p;
					DynamicAllocation fft_cbuffer_allocation = dynamic_allocator->Allocate(GetCBufferSize<FFTCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					fft_cbuffer_allocation.Update(fft_cbuf_data);
					cmd_list->SetComputeRootConstantBufferView(0, fft_cbuffer_allocation.gpu_address);

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
						context.GetReadOnlyTexture(data.spectrum_srv),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1),
						context.GetReadWriteTexture(data.spectrum_uav),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index + 1));
					cmd_list->Dispatch(FFT_RESOLUTION, 1, 1);

				}, ERGPassType::Compute, ERGPassFlags::None);
			pong_spectrum = !pong_spectrum;
		}

		//Add Vertical FFT
		for (uint32 p = 1; p < FFT_RESOLUTION; p <<= 1)
		{
			struct FFTVerticalPassData
			{
				RGTextureReadOnlyId spectrum_srv;
				RGTextureReadWriteId spectrum_uav;
			};

			rendergraph.AddPass<FFTVerticalPassData>("FFT Vertical Pass",
				[=, pong_spectrum = this->pong_spectrum](FFTVerticalPassData& data, RenderGraphBuilder& builder)
				{
					RGResourceName pong_spectrum_texture = !pong_spectrum ? RG_RES_NAME(PongSpectrum) : RG_RES_NAME(PingSpectrum);
					RGResourceName ping_spectrum_texture = !pong_spectrum ? RG_RES_NAME(PingSpectrum) : RG_RES_NAME(PongSpectrum);
					data.spectrum_srv = builder.ReadTexture(pong_spectrum_texture, ReadAccess_NonPixelShader);
					data.spectrum_uav = builder.WriteTexture(ping_spectrum_texture);
				},
				[=](FFTVerticalPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
				{
					auto device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
					auto dynamic_allocator = gfx->GetDynamicAllocator();

					cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::FFT));
					cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::FFT_Vertical));

					FFTCBuffer fft_cbuf_data{ .seq_count = FFT_RESOLUTION };
					fft_cbuf_data.subseq_count = p;
					DynamicAllocation fft_cbuffer_allocation = dynamic_allocator->Allocate(GetCBufferSize<FFTCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
					fft_cbuffer_allocation.Update(fft_cbuf_data);
					cmd_list->SetComputeRootConstantBufferView(0, fft_cbuffer_allocation.gpu_address);

					OffsetType descriptor_index = descriptor_allocator->AllocateRange(2);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
						context.GetReadOnlyTexture(data.spectrum_srv),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index + 1),
						context.GetReadWriteTexture(data.spectrum_uav),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index + 1));
					cmd_list->Dispatch(FFT_RESOLUTION, 1, 1);

				}, ERGPassType::Compute, ERGPassFlags::None);
			pong_spectrum = !pong_spectrum;
		}

		struct OceanNormalsPassData
		{
			RGTextureReadOnlyId spectrum_srv;
			RGTextureReadWriteId normals_uav;
		};
		rendergraph.AddPass<OceanNormalsPassData>("Ocean Normals Pass",
			[=](OceanNormalsPassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc ocean_desc{};
				ocean_desc.width = FFT_RESOLUTION;
				ocean_desc.height = FFT_RESOLUTION;
				ocean_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(OceanNormals), ocean_desc);

				RGResourceName pong_spectrum_texture = !pong_spectrum ? RG_RES_NAME(PongSpectrum) : RG_RES_NAME(PingSpectrum);
				data.spectrum_srv = builder.ReadTexture(pong_spectrum_texture, ReadAccess_NonPixelShader);
				data.normals_uav = builder.WriteTexture(RG_RES_NAME(OceanNormals));
			},
			[=](OceanNormalsPassData const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::OceanNormalMap));
				cmd_list->SetPipelineState(PSOCache::Get(EPipelineState::OceanNormalMap));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.compute_cbuffer_address);

				OffsetType descriptor_index = descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
					context.GetReadOnlyTexture(data.spectrum_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(descriptor_index));

				descriptor_index = descriptor_allocator->Allocate();
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(descriptor_index),
					context.GetReadWriteTexture(data.normals_uav),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetHandle(descriptor_index));
				cmd_list->Dispatch(FFT_RESOLUTION / 32, FFT_RESOLUTION / 32, 1);
			}, ERGPassType::Compute, ERGPassFlags::None);

		struct OceanDrawPass
		{
			RGTextureReadOnlyId normals_srv;
			RGTextureReadOnlyId spectrum_srv;
		};

		rendergraph.AddPass<OceanDrawPass>("Ocean Draw Pass",
			[=](OceanDrawPass& data, RenderGraphBuilder& builder)
			{
				RGResourceName ping_spectrum_texture = !pong_spectrum ? RG_RES_NAME(PingSpectrum) : RG_RES_NAME(PongSpectrum);
				data.spectrum_srv = builder.ReadTexture(ping_spectrum_texture, ReadAccess_NonPixelShader);
				data.normals_srv = builder.ReadTexture(RG_RES_NAME(OceanNormals), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), ERGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](OceanDrawPass const& data, RenderGraphContext& context, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				auto skyboxes = reg.view<Skybox>();
				D3D12_CPU_DESCRIPTOR_HANDLE skybox_handle = global_data.null_srv_texturecube;
				for (auto skybox : skyboxes)
				{
					auto const& _skybox = skyboxes.get(skybox);
					if (_skybox.active)
					{
						skybox_handle = texture_manager.CpuDescriptorHandle(_skybox.cubemap_texture);
						break;
					}
				}

				Descriptor displacement_handle = context.GetReadOnlyTexture(data.spectrum_srv);
				if (tesselated)
				{
					cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::OceanLOD));
					cmd_list->SetPipelineState(
						wireframe ? PSOCache::Get(EPipelineState::OceanLOD_Wireframe) :
						PSOCache::Get(EPipelineState::OceanLOD));
				}
				else
				{
					cmd_list->SetGraphicsRootSignature(RootSignatureCache::Get(ERootSignature::Ocean));
					cmd_list->SetPipelineState(
						wireframe ? PSOCache::Get(EPipelineState::Ocean_Wireframe) :
						PSOCache::Get(EPipelineState::Ocean));
				}
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetGraphicsRootConstantBufferView(3, global_data.weather_cbuffer_address);

				auto ocean_chunk_view = reg.view<Mesh, Material, Transform, Visibility, Ocean>();
				ObjectCBuffer object_cbuf_data{};
				MaterialCBuffer material_cbuf_data{};
				for (auto ocean_chunk : ocean_chunk_view)
				{
					auto const& [mesh, material, transform, visibility] = ocean_chunk_view.get<const Mesh, const Material, const Transform, const Visibility>(ocean_chunk);

					if (visibility.camera_visible)
					{
						object_cbuf_data.model = transform.current_transform;
						object_cbuf_data.inverse_transposed_model = XMMatrixInverse(nullptr, object_cbuf_data.model);
						DynamicAllocation object_allocation = dynamic_allocator->Allocate(GetCBufferSize<ObjectCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						object_allocation.Update(object_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(1, object_allocation.gpu_address);

						material_cbuf_data.diffuse = material.diffuse;
						DynamicAllocation material_allocation = dynamic_allocator->Allocate(GetCBufferSize<MaterialCBuffer>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						material_allocation.Update(material_cbuf_data);
						cmd_list->SetGraphicsRootConstantBufferView(2, material_allocation.gpu_address);

						OffsetType descriptor_index = descriptor_allocator->Allocate();
						auto dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
						device->CopyDescriptorsSimple(1, dst_descriptor, displacement_handle,
							D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						cmd_list->SetGraphicsRootDescriptorTable(4, dst_descriptor);

						descriptor_index = descriptor_allocator->AllocateRange(3);
						dst_descriptor = descriptor_allocator->GetHandle(descriptor_index);
						D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadOnlyTexture(data.normals_srv), skybox_handle, texture_manager.CpuDescriptorHandle(foam_handle) };
						D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { dst_descriptor };
						UINT src_range_sizes[] = { 1, 1, 1 };
						UINT dst_range_sizes[] = { 3 };
						device->CopyDescriptors(1, dst_ranges, dst_range_sizes, 3, src_ranges, src_range_sizes,
							D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						cmd_list->SetGraphicsRootDescriptorTable(5, dst_descriptor);

						tesselated ? mesh.Draw(cmd_list, D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST) : mesh.Draw(cmd_list);
					}
				}
			}, 
			ERGPassType::Graphics, ERGPassFlags::None);
	}

	void OceanRenderer::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void OceanRenderer::OnSceneInitialized(GraphicsDevice* gfx)
	{
		foam_handle = texture_manager.LoadTexture(L"Resources/Textures/foam.jpg");
		perlin_handle = texture_manager.LoadTexture(L"Resources/Textures/perlin.dds");

		TextureDesc ocean_texture_desc{};
		ocean_texture_desc.width = FFT_RESOLUTION;
		ocean_texture_desc.height = FFT_RESOLUTION;
		ocean_texture_desc.format = DXGI_FORMAT_R32_FLOAT;
		ocean_texture_desc.bind_flags = EBindFlag::ShaderResource | EBindFlag::UnorderedAccess;
		ocean_texture_desc.initial_state = EResourceState::UnorderedAccess;
		initial_spectrum = std::make_unique<Texture>(gfx, ocean_texture_desc);

		std::vector<float32> ping_array(FFT_RESOLUTION * FFT_RESOLUTION);
		RealRandomGenerator rand_float{ 0.0f,  2.0f * pi<float32> };
		for (size_t i = 0; i < ping_array.size(); ++i) ping_array[i] = rand_float();

		TextureInitialData data{};
		data.pData = ping_array.data();
		data.RowPitch = sizeof(float32) * FFT_RESOLUTION;
		data.SlicePitch = 0;

		ping_pong_phase_textures[pong_phase] = std::make_unique<Texture>(gfx, ocean_texture_desc, &data);
		ping_pong_phase_textures[!pong_phase] = std::make_unique<Texture>(gfx, ocean_texture_desc);

		ocean_texture_desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		ping_pong_spectrum_textures[pong_spectrum] = std::make_unique<Texture>(gfx, ocean_texture_desc);
		ping_pong_spectrum_textures[!pong_spectrum] = std::make_unique<Texture>(gfx, ocean_texture_desc);
	}

}

