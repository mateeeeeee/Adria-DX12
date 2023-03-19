#include "OceanRenderer.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GfxTexture.h"
#include "../Graphics/RingGPUDescriptorAllocator.h"
#include "../Graphics/LinearDynamicAllocator.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/GfxCommon.h"
#include "../Editor/GUICommand.h"
#include "../Utilities/Random.h"
#include "../Math/Constants.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	OceanRenderer::OceanRenderer(entt::registry& reg, uint32 w, uint32 h)
		: reg{ reg }, width{ w }, height{ h }
	{}

	void OceanRenderer::AddPasses(RenderGraph& rendergraph)
	{
		if (reg.view<Ocean>().size() == 0) return;
		FrameBlackboardData const& global_data = rendergraph.GetBlackboard().GetChecked<FrameBlackboardData>();

		if (ocean_color_changed)
		{
			auto ocean_view = reg.view<Ocean, Material>();
			for (auto e : ocean_view)
			{
				auto& material = ocean_view.get<Material>(e);
				memcpy(material.base_color, ocean_color, 3 * sizeof(ocean_color));
			}
		}

		rendergraph.ImportTexture(RG_RES_NAME(InitialSpectrum), initial_spectrum.get());
		rendergraph.ImportTexture(RG_RES_NAME(PongPhase), ping_pong_phase_textures[pong_phase].get());
		rendergraph.ImportTexture(RG_RES_NAME(PingPhase), ping_pong_phase_textures[!pong_phase].get());
		rendergraph.ImportTexture(RG_RES_NAME(PongSpectrum), ping_pong_spectrum_textures[pong_spectrum].get());
		rendergraph.ImportTexture(RG_RES_NAME(PingSpectrum), ping_pong_spectrum_textures[!pong_spectrum].get());

		if (recreate_initial_spectrum)
		{ 
			struct InitialSpectrumPassData
			{
				RGTextureReadWriteId initial_spectrum;
			};
			rendergraph.AddPass<InitialSpectrumPassData>("Spectrum Creation Pass",
				[=](InitialSpectrumPassData& data, RenderGraphBuilder& builder)
				{
					data.initial_spectrum = builder.WriteTexture(RG_RES_NAME(InitialSpectrum));
				},
				[=](InitialSpectrumPassData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
				{
					auto device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
					uint32 i = (uint32)descriptor_allocator->Allocate();
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadWriteTexture(data.initial_spectrum),
						D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					struct InitialSpectrumConstants
					{
						float   fft_resolution;
						float   ocean_size;
						uint32  output_idx;
					} constants = 
					{
						.fft_resolution = FFT_RESOLUTION, .ocean_size = FFT_RESOLUTION, .output_idx = i
					};

					
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::InitialSpectrum));
					cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
					cmd_list->SetComputeRoot32BitConstants(1, 3, &constants, 0);
					cmd_list->Dispatch(FFT_RESOLUTION / 16, FFT_RESOLUTION / 16, 1);
				}, RGPassType::Compute, RGPassFlags::None);
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
			[=](PhasePassData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadOnlyTexture(data.phase_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), context.GetReadWriteTexture(data.phase_uav),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct PhaseConstants
				{
					float   fft_resolution;
					float   ocean_size;
					float   ocean_choppiness;
					uint32  phases_idx;
					uint32  output_idx;
				} constants =
				{
					.fft_resolution = FFT_RESOLUTION, .ocean_size = FFT_RESOLUTION, .ocean_choppiness = ocean_choppiness,
					.phases_idx = i, .output_idx = i + 1
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Phase));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 5, &constants, 0);
				cmd_list->Dispatch(FFT_RESOLUTION / 16, FFT_RESOLUTION / 16, 1);
			}, RGPassType::Compute, RGPassFlags::None);
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
			[=](SpectrumPassData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				uint32 i = (uint32)descriptor_allocator->AllocateRange(3);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadOnlyTexture(data.initial_spectrum_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), context.GetReadOnlyTexture(data.phase_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), context.GetReadWriteTexture(data.spectrum_uav),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct SpectrumConstants
				{
					float   fft_resolution;
					float   ocean_size;
					float   ocean_choppiness;
					uint32  initial_spectrum_idx;
					uint32  phases_idx;
					uint32  output_idx;
				} constants =
				{
					.fft_resolution = FFT_RESOLUTION, .ocean_size = FFT_RESOLUTION, .ocean_choppiness = ocean_choppiness,
					.initial_spectrum_idx = i, .phases_idx = i + 1, .output_idx = i + 2
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::Spectrum));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 6, &constants, 0);
				cmd_list->Dispatch(FFT_RESOLUTION / 16, FFT_RESOLUTION / 16, 1);

			}, RGPassType::Compute, RGPassFlags::None);

		struct FFTConstants
		{
			uint32 input_idx;
			uint32 output_idx;
			uint32 seq_count;
			uint32 subseq_count;
		};

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
				[=](FFTHorizontalPassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, CommandList* cmd_list)
				{
					auto device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

					
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::FFT_Horizontal));

					uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), ctx.GetReadOnlyTexture(data.spectrum_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.spectrum_uav), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					
					FFTConstants fft_constants{};
					fft_constants.seq_count = FFT_RESOLUTION;
					fft_constants.subseq_count = p;
					fft_constants.input_idx = i;
					fft_constants.output_idx = i + 1;

					cmd_list->SetComputeRoot32BitConstants(1, 4, &fft_constants, 0);
					cmd_list->Dispatch(FFT_RESOLUTION, 1, 1);

				}, RGPassType::Compute, RGPassFlags::None);
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
				[=](FFTVerticalPassData const& data, RenderGraphContext& ctx, GfxDevice* gfx, CommandList* cmd_list)
				{
					auto device = gfx->GetDevice();
					auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

					
					cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::FFT_Vertical));

					uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), ctx.GetReadOnlyTexture(data.spectrum_srv), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.spectrum_uav), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

					FFTConstants fft_constants{};
					fft_constants.seq_count = FFT_RESOLUTION;
					fft_constants.subseq_count = p;
					fft_constants.input_idx = i;
					fft_constants.output_idx = i + 1;

					cmd_list->SetComputeRoot32BitConstants(1, 4, &fft_constants, 0);
					cmd_list->Dispatch(FFT_RESOLUTION, 1, 1);

				}, RGPassType::Compute, RGPassFlags::None);
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
				ocean_desc.format = GfxFormat::R32G32B32A32_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(OceanNormals), ocean_desc);

				RGResourceName pong_spectrum_texture = !pong_spectrum ? RG_RES_NAME(PongSpectrum) : RG_RES_NAME(PingSpectrum);
				data.spectrum_srv = builder.ReadTexture(pong_spectrum_texture, ReadAccess_NonPixelShader);
				data.normals_uav = builder.WriteTexture(RG_RES_NAME(OceanNormals));
			},
			[=](OceanNormalsPassData const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), context.GetReadOnlyTexture(data.spectrum_srv),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), context.GetReadWriteTexture(data.normals_uav),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct OceanNormalsConstants
				{
					float   fft_resolution;
					float   ocean_size;
					float   ocean_choppiness;
					uint32  displacement_idx;
					uint32  output_idx;
				} constants =
				{
					.fft_resolution = FFT_RESOLUTION, .ocean_size = FFT_RESOLUTION, .ocean_choppiness = ocean_choppiness,
					.displacement_idx = i, .output_idx = i + 1
				};

				
				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::OceanNormals));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 5, &constants, 0);
				cmd_list->Dispatch(FFT_RESOLUTION / 16, FFT_RESOLUTION / 16, 1);
			}, RGPassType::Compute, RGPassFlags::None);

		struct OceanDrawPass
		{
			RGTextureReadOnlyId normals;
			RGTextureReadOnlyId displacement;
		};

		rendergraph.AddPass<OceanDrawPass>("Ocean Draw Pass",
			[=](OceanDrawPass& data, RenderGraphBuilder& builder)
			{
				RGResourceName ping_spectrum_texture = !pong_spectrum ? RG_RES_NAME(PingSpectrum) : RG_RES_NAME(PongSpectrum);
				data.displacement = builder.ReadTexture(ping_spectrum_texture, ReadAccess_NonPixelShader);
				data.normals = builder.ReadTexture(RG_RES_NAME(OceanNormals), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](OceanDrawPass const& data, RenderGraphContext& context, GfxDevice* gfx, CommandList* cmd_list)
			{
				ID3D12Device* device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				auto skyboxes = reg.view<Skybox>();
				D3D12_CPU_DESCRIPTOR_HANDLE skybox_handle = gfxcommon::GetCommonView(GfxCommonViewType::NullTextureCube_SRV);
				for (auto skybox : skyboxes)
				{
					auto const& _skybox = skyboxes.get<Skybox>(skybox);
					if (_skybox.active)
					{
						skybox_handle = TextureManager::Get().GetSRV(_skybox.cubemap_texture);
						break;
					}
				}
				
				if (ocean_tesselation)
				{
					cmd_list->SetPipelineState(
						ocean_tesselation ? PSOCache::Get(GfxPipelineStateID::OceanLOD_Wireframe) :
						PSOCache::Get(GfxPipelineStateID::OceanLOD));
				}
				else
				{
					cmd_list->SetPipelineState(
						ocean_wireframe ? PSOCache::Get(GfxPipelineStateID::Ocean_Wireframe) :
						PSOCache::Get(GfxPipelineStateID::Ocean));
				}
				cmd_list->SetGraphicsRootConstantBufferView(0, global_data.frame_cbuffer_address);

				auto ocean_chunk_view = reg.view<Mesh, Material, Transform, AABB, Ocean>();				
				for (auto ocean_chunk : ocean_chunk_view)
				{
					auto const& [mesh, material, transform, aabb] = ocean_chunk_view.get<const Mesh, const Material, const Transform, const AABB>(ocean_chunk);

					if (aabb.camera_visible)
					{
						uint32 i = (uint32)descriptor_allocator->AllocateRange(4);
						auto dst_descriptor = descriptor_allocator->GetHandle(i);
						D3D12_CPU_DESCRIPTOR_HANDLE src_ranges[] = { context.GetReadOnlyTexture(data.displacement), context.GetReadOnlyTexture(data.normals), skybox_handle, TextureManager::Get().GetSRV(foam_handle) };
						D3D12_CPU_DESCRIPTOR_HANDLE dst_ranges[] = { dst_descriptor };
						UINT src_range_sizes[] = { 1, 1, 1, 1 };
						UINT dst_range_sizes[] = { ARRAYSIZE(src_ranges) };
						device->CopyDescriptors(1, dst_ranges, dst_range_sizes, ARRAYSIZE(src_ranges), src_ranges, src_range_sizes,
							D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

						struct OceanIndices
						{
							uint32 displacement_idx;
							uint32 normal_idx;
							uint32 sky_idx;
							uint32 foam_idx;
						} indices = 
						{
							.displacement_idx = i, .normal_idx = i + 1,
							.sky_idx = i + 2, .foam_idx = i + 3
						};

						struct OceanConstants
						{
							XMMATRIX ocean_model_matrix;
							XMFLOAT3 ocean_color;
						} constants = 
						{
							.ocean_model_matrix = transform.current_transform,
							.ocean_color = XMFLOAT3(material.base_color)
						};
						DynamicAllocation allocation = dynamic_allocator->Allocate(GetCBufferSize<OceanConstants>(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
						allocation.Update(constants);

						cmd_list->SetGraphicsRoot32BitConstants(1, 4, &indices, 0);
						cmd_list->SetGraphicsRootConstantBufferView(2, allocation.gpu_address);
						ocean_tesselation ? mesh.Draw(cmd_list, D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST) : mesh.Draw(cmd_list);
					}
				}
			}, 
			RGPassType::Graphics, RGPassFlags::None);

		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Ocean Settings", 0))
				{
					ImGui::Checkbox("Tessellation", &ocean_tesselation);
					ImGui::Checkbox("Wireframe", &ocean_wireframe);

					ImGui::SliderFloat("Choppiness", &ocean_choppiness, 0.0f, 10.0f);
					ocean_color_changed = ImGui::ColorEdit3("Ocean Color", ocean_color);
					recreate_initial_spectrum = ImGui::SliderFloat2("Wind Direction", wind_direction, 0.0f, 50.0f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			});
	}

	void OceanRenderer::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void OceanRenderer::OnSceneInitialized(GfxDevice* gfx)
	{
		foam_handle = TextureManager::Get().LoadTexture(L"Resources/Textures/foam.jpg");
		perlin_handle = TextureManager::Get().LoadTexture(L"Resources/Textures/perlin.dds");

		GfxTextureDesc ocean_texture_desc{};
		ocean_texture_desc.width = FFT_RESOLUTION;
		ocean_texture_desc.height = FFT_RESOLUTION;
		ocean_texture_desc.format = GfxFormat::R32_FLOAT;
		ocean_texture_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		ocean_texture_desc.initial_state = GfxResourceState::UnorderedAccess;
		initial_spectrum = std::make_unique<GfxTexture>(gfx, ocean_texture_desc);

		std::vector<float> ping_array(FFT_RESOLUTION * FFT_RESOLUTION);
		RealRandomGenerator rand_float{ 0.0f,  2.0f * pi<float> };
		for (size_t i = 0; i < ping_array.size(); ++i) ping_array[i] = rand_float();

		GfxTextureInitialData data{};
		data.pData = ping_array.data();
		data.RowPitch = sizeof(float) * FFT_RESOLUTION;
		data.SlicePitch = 0;

		ping_pong_phase_textures[pong_phase] = std::make_unique<GfxTexture>(gfx, ocean_texture_desc, &data);
		ping_pong_phase_textures[!pong_phase] = std::make_unique<GfxTexture>(gfx, ocean_texture_desc);

		ocean_texture_desc.format = GfxFormat::R32G32B32A32_FLOAT;
		ping_pong_spectrum_textures[pong_spectrum] = std::make_unique<GfxTexture>(gfx, ocean_texture_desc);
		ping_pong_spectrum_textures[!pong_spectrum] = std::make_unique<GfxTexture>(gfx, ocean_texture_desc);
	}

}

