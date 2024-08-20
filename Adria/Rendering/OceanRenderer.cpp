#include "OceanRenderer.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "Core/Paths.h"
#include "RenderGraph/RenderGraph.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "Graphics/GfxReflection.h"
#include "Graphics/GfxCommon.h"
#include "Editor/GUICommand.h"
#include "Utilities/Random.h"
#include "Math/Constants.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	OceanRenderer::OceanRenderer(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h)
		: reg{ reg }, gfx{ gfx }, width{ w }, height{ h }
	{
		CreatePSOs();
	}

	OceanRenderer::~OceanRenderer() = default;

	void OceanRenderer::AddPasses(RenderGraph& rendergraph)
	{
		if (reg.view<Ocean>().size() == 0) return;
		FrameBlackboardData const& frame_data = rendergraph.GetBlackboard().Get<FrameBlackboardData>();

		if (ocean_color_changed)
		{
			auto ocean_view = reg.view<Ocean, Material>();
			for (auto e : ocean_view)
			{
				auto& material = ocean_view.get<Material>(e);
				memcpy(material.base_color, ocean_color, sizeof(ocean_color));
			}
		}

		rendergraph.ImportTexture(RG_NAME(InitialSpectrum), initial_spectrum.get());
		rendergraph.ImportTexture(RG_NAME(PongPhase), ping_pong_phase_textures[pong_phase].get());
		rendergraph.ImportTexture(RG_NAME(PingPhase), ping_pong_phase_textures[!pong_phase].get());
		rendergraph.ImportTexture(RG_NAME(PongSpectrum), ping_pong_spectrum_textures[pong_spectrum].get());
		rendergraph.ImportTexture(RG_NAME(PingSpectrum), ping_pong_spectrum_textures[!pong_spectrum].get());

		if (recreate_initial_spectrum)
		{
			struct InitialSpectrumPassData
			{
				RGTextureReadWriteId initial_spectrum;
			};
			rendergraph.AddPass<InitialSpectrumPassData>("Spectrum Creation Pass",
				[=](InitialSpectrumPassData& data, RenderGraphBuilder& builder)
				{
					data.initial_spectrum = builder.WriteTexture(RG_NAME(InitialSpectrum));
				},
				[=](InitialSpectrumPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
				{
					GfxDevice* gfx = cmd_list->GetDevice();
					
					GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU();
					gfx->CopyDescriptors(1, dst_descriptor, context.GetReadWriteTexture(data.initial_spectrum));

					struct InitialSpectrumConstants
					{
						float   fft_resolution;
						float   ocean_size;
						uint32  output_idx;
					} constants =
					{
						.fft_resolution = FFT_RESOLUTION, .ocean_size = FFT_RESOLUTION, .output_idx = dst_descriptor.GetIndex()
					};

					cmd_list->SetPipelineState(initial_spectrum_pso.get());
					cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
					cmd_list->SetRootConstants(1, constants);
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
				data.phase_srv = builder.ReadTexture(RG_NAME(PongPhase), ReadAccess_NonPixelShader);
				data.phase_uav = builder.WriteTexture(RG_NAME(PingPhase));
			},
			[=](PhasePassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), context.GetReadOnlyTexture(data.phase_srv));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), context.GetReadWriteTexture(data.phase_uav));

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

				cmd_list->SetPipelineState(phase_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
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
				data.phase_srv = builder.ReadTexture(RG_NAME(PongPhase), ReadAccess_NonPixelShader);
				data.initial_spectrum_srv = builder.ReadTexture(RG_NAME(InitialSpectrum), ReadAccess_NonPixelShader);
				data.spectrum_uav = builder.WriteTexture(RG_NAME(PongSpectrum));
			},
			[=](SpectrumPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				uint32 i = gfx->AllocateDescriptorsGPU(3).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), context.GetReadOnlyTexture(data.initial_spectrum_srv));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), context.GetReadOnlyTexture(data.phase_srv));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 2), context.GetReadWriteTexture(data.spectrum_uav));

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


				cmd_list->SetPipelineState(spectrum_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
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
					RGResourceName pong_spectrum_texture = !pong_spectrum ? RG_NAME(PongSpectrum) : RG_NAME(PingSpectrum);
					RGResourceName ping_spectrum_texture = !pong_spectrum ? RG_NAME(PingSpectrum) : RG_NAME(PongSpectrum);
					data.spectrum_srv = builder.ReadTexture(pong_spectrum_texture, ReadAccess_NonPixelShader);
					data.spectrum_uav = builder.WriteTexture(ping_spectrum_texture);
				},
				[=](FFTHorizontalPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
				{
					GfxDevice* gfx = cmd_list->GetDevice();
					

					cmd_list->SetPipelineState(fft_horizontal_pso.get());

					uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
					gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), ctx.GetReadOnlyTexture(data.spectrum_srv));
					gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadWriteTexture(data.spectrum_uav));

					FFTConstants fft_constants{};
					fft_constants.seq_count = FFT_RESOLUTION;
					fft_constants.subseq_count = p;
					fft_constants.input_idx = i;
					fft_constants.output_idx = i + 1;

					cmd_list->SetRootConstants(1, fft_constants);
					cmd_list->Dispatch(FFT_RESOLUTION, 1, 1);

				}, RGPassType::Compute, RGPassFlags::None);
			pong_spectrum = !pong_spectrum;
		}

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
					RGResourceName pong_spectrum_texture = !pong_spectrum ? RG_NAME(PongSpectrum) : RG_NAME(PingSpectrum);
					RGResourceName ping_spectrum_texture = !pong_spectrum ? RG_NAME(PingSpectrum) : RG_NAME(PongSpectrum);
					data.spectrum_srv = builder.ReadTexture(pong_spectrum_texture, ReadAccess_NonPixelShader);
					data.spectrum_uav = builder.WriteTexture(ping_spectrum_texture);
				},
				[=](FFTVerticalPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
				{
					GfxDevice* gfx = cmd_list->GetDevice();
					

					cmd_list->SetPipelineState(fft_vertical_pso.get());

					uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
					gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), ctx.GetReadOnlyTexture(data.spectrum_srv));
					gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), ctx.GetReadWriteTexture(data.spectrum_uav));

					FFTConstants fft_constants{};
					fft_constants.seq_count = FFT_RESOLUTION;
					fft_constants.subseq_count = p;
					fft_constants.input_idx = i;
					fft_constants.output_idx = i + 1;

					cmd_list->SetRootConstants(1, fft_constants);
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
				builder.DeclareTexture(RG_NAME(OceanNormals), ocean_desc);

				RGResourceName pong_spectrum_texture = !pong_spectrum ? RG_NAME(PongSpectrum) : RG_NAME(PingSpectrum);
				data.spectrum_srv = builder.ReadTexture(pong_spectrum_texture, ReadAccess_NonPixelShader);
				data.normals_uav = builder.WriteTexture(RG_NAME(OceanNormals));
			},
			[=](OceanNormalsPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				uint32 i = gfx->AllocateDescriptorsGPU(2).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), context.GetReadOnlyTexture(data.spectrum_srv));
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 1), context.GetReadWriteTexture(data.normals_uav));

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

				cmd_list->SetPipelineState(ocean_normals_pso.get());
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(FFT_RESOLUTION / 16, FFT_RESOLUTION / 16, 1);
			}, RGPassType::Compute, RGPassFlags::None);

		struct OceanDrawPassData
		{
			RGTextureReadOnlyId normals;
			RGTextureReadOnlyId displacement;
		};

		rendergraph.AddPass<OceanDrawPassData>("Ocean Draw Pass",
			[=](OceanDrawPassData& data, RenderGraphBuilder& builder)
			{
				RGResourceName ping_spectrum_texture = !pong_spectrum ? RG_NAME(PingSpectrum) : RG_NAME(PongSpectrum);
				data.displacement = builder.ReadTexture(ping_spectrum_texture, ReadAccess_NonPixelShader);
				data.normals = builder.ReadTexture(RG_NAME(OceanNormals), ReadAccess_PixelShader);
				builder.WriteRenderTarget(RG_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](OceanDrawPassData const& data, RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				if (ocean_tesselation)
				{
					cmd_list->SetPipelineState(
						ocean_wireframe ? ocean_lod_psos->Get<1>() :
										  ocean_lod_psos->Get<0>());
				}
				else
				{
					cmd_list->SetPipelineState(
						ocean_wireframe ? ocean_psos->Get<1>() :
										  ocean_psos->Get<0>());
				}
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);

				auto ocean_chunk_view = reg.view<SubMesh, Material, Transform, Ocean>();
				for (auto ocean_chunk : ocean_chunk_view)
				{
					auto const& [mesh, material, transform] = ocean_chunk_view.get<const SubMesh, const Material, const Transform>(ocean_chunk);

					uint32 i = gfx->AllocateDescriptorsGPU(3).GetIndex();
					GfxDescriptor dst_handle = gfx->GetDescriptorGPU(i);
					GfxDescriptor src_handles[] = { context.GetReadOnlyTexture(data.displacement), context.GetReadOnlyTexture(data.normals), g_TextureManager.GetSRV(foam_handle) };
					gfx->CopyDescriptors(dst_handle, src_handles);

					struct OceanIndices
					{
						uint32 displacement_idx;
						uint32 normal_idx;
						uint32 foam_idx;
					} indices =
					{
						.displacement_idx = i, .normal_idx = i + 1,
						.foam_idx = i + 3
					};

					struct OceanConstants
					{
						Matrix ocean_model_matrix;
						Vector3 ocean_color;
					} constants =
					{
						.ocean_model_matrix = transform.current_transform,
						.ocean_color = Vector3(material.base_color)
					};

					cmd_list->SetRootConstants(1, indices);
					cmd_list->SetRootCBV(2, constants);
					ocean_tesselation ? Draw(mesh, cmd_list, true, GfxPrimitiveTopology::PatchList3) : Draw(mesh, cmd_list);
				}
			},
			RGPassType::Graphics, RGPassFlags::None);
	}

	void OceanRenderer::GUI()
	{
		QueueGUI([&]()
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
			}, GUICommandGroup_Renderer);
	}

	void OceanRenderer::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void OceanRenderer::OnSceneInitialized()
	{
		foam_handle = g_TextureManager.LoadTexture(paths::TexturesDir + "Ocean/Foam.jpg");
		perlin_handle = g_TextureManager.LoadTexture(paths::TexturesDir + "Ocean/Perlin.png");

		GfxTextureDesc ocean_texture_desc{};
		ocean_texture_desc.width = FFT_RESOLUTION;
		ocean_texture_desc.height = FFT_RESOLUTION;
		ocean_texture_desc.format = GfxFormat::R32_FLOAT;
		ocean_texture_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		ocean_texture_desc.initial_state = GfxResourceState::ComputeUAV;
		initial_spectrum = gfx->CreateTexture(ocean_texture_desc);

		std::vector<float> ping_array(FFT_RESOLUTION * FFT_RESOLUTION);
		RealRandomGenerator rand_float{ 0.0f,  2.0f * pi<float> };
		for (uint64 i = 0; i < ping_array.size(); ++i) ping_array[i] = rand_float();

		GfxTextureSubData data{};
		data.data = ping_array.data();
		data.row_pitch = sizeof(float) * FFT_RESOLUTION;
		data.slice_pitch = 0;

		GfxTextureData init_data{};
		init_data.sub_data = &data;
		init_data.sub_count = 1;

		ping_pong_phase_textures[pong_phase] = gfx->CreateTexture(ocean_texture_desc, init_data);
		ping_pong_phase_textures[!pong_phase] = gfx->CreateTexture(ocean_texture_desc);

		ocean_texture_desc.format = GfxFormat::R32G32B32A32_FLOAT;
		ping_pong_spectrum_textures[pong_spectrum] = gfx->CreateTexture(ocean_texture_desc);
		ping_pong_spectrum_textures[!pong_spectrum] = gfx->CreateTexture(ocean_texture_desc);
	}

	void OceanRenderer::CreatePSOs()
	{
		using enum GfxShaderStage;
		GfxGraphicsPipelineStateDesc gfx_pso_desc{};
		GfxReflection::FillInputLayoutDesc(GetGfxShader(VS_Ocean), gfx_pso_desc.input_layout);
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_Ocean;
		gfx_pso_desc.PS = PS_Ocean;
		gfx_pso_desc.depth_state.depth_enable = true;
		gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
		gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		gfx_pso_desc.num_render_targets = 1;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
		gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;

		ocean_psos = std::make_unique<GfxGraphicsPipelineStatePermutations>(2, gfx_pso_desc);
		ocean_psos->SetFillMode<1>(GfxFillMode::Wireframe);
		ocean_psos->Finalize(gfx);

		gfx_pso_desc.VS = VS_OceanLOD;
		gfx_pso_desc.DS = DS_OceanLOD;
		gfx_pso_desc.HS = HS_OceanLOD;
		gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Patch;
		ocean_lod_psos = std::make_unique<GfxGraphicsPipelineStatePermutations>(2, gfx_pso_desc);
		ocean_lod_psos->SetFillMode<1>(GfxFillMode::Wireframe);
		ocean_lod_psos->Finalize(gfx);

		GfxComputePipelineStateDesc compute_pso_desc{};
		compute_pso_desc.CS = CS_FFT_Horizontal;
		fft_horizontal_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_FFT_Vertical;
		fft_vertical_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_InitialSpectrum;
		initial_spectrum_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_Spectrum;
		spectrum_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_Phase;
		phase_pso = gfx->CreateComputePipelineState(compute_pso_desc);

		compute_pso_desc.CS = CS_OceanNormals;
		ocean_normals_pso = gfx->CreateComputePipelineState(compute_pso_desc);
	}

}

