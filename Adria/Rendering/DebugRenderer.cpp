#include "DebugRenderer.h"
#include "BlackboardData.h"
#include "ShaderManager.h"
#include "Graphics/GfxDynamicAllocation.h"
#include "Graphics/GfxReflection.h"
#include "Graphics/GfxPipelineStatePermutations.h"
#include "RenderGraph/RenderGraph.h"
#include "Math/Packing.h"
#include "Math/Constants.h"

namespace adria
{
	struct SpherePointHelper
	{
		Vector3 center;
		float radius;

		Vector3 operator()(float theta, float phi) const
		{
			return center + Vector3(
				radius * sin(theta) * sin(phi),
				radius * cos(phi),
				radius * cos(theta) * sin(phi)
			);
		}
	};

	uint32 DebugRenderer::ColorToUint(Color const& col)
	{
		return PackToUint(col.R(), col.G(), col.B(), col.A());
	}

	void DebugRenderer::Initialize(GfxDevice* _gfx, uint32 _width, uint32 _height)
	{
		gfx = _gfx;
		width = _width;
		height = _height; 
		CreatePSOs();
	}

	void DebugRenderer::Destroy()
	{
		transient_lines.clear();
		transient_triangles.clear();
		width = 0, height = 0;
		gfx = nullptr;
	}

	void DebugRenderer::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void DebugRenderer::Render(RenderGraph& rg)
	{
		if (transient_lines.empty() && transient_triangles.empty() && persistent_lines.empty() && persistent_triangles.empty()) 
			return;

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		rg.AddPass<void>("Debug Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_NAME(FinalTexture), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.ReadDepthStencil(RG_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				cmd_list->SetRootCBV(0, frame_data.frame_cbuffer_address);

				constexpr uint32 vb_stride = (uint32)sizeof(DebugVertex);
				uint32 transient_lines_count = (uint32)transient_lines.size();
				uint32 persistent_lines_count = (uint32)persistent_lines.size();
				uint32 lines_count = transient_lines_count + persistent_lines_count;

				uint32 transient_triangles_count = (uint32)transient_triangles.size();
				uint32 persistent_triangles_count = (uint32)persistent_triangles.size();
				uint32 triangles_count = transient_triangles_count + persistent_triangles_count;

				if (lines_count != 0)
				{
					uint32 transient_vb_count = transient_lines_count * sizeof(DebugLine) / vb_stride;
					uint32 persistent_vb_count = persistent_lines_count * sizeof(DebugLine) / vb_stride;
					uint32 vb_count = transient_vb_count + persistent_vb_count;
					GfxDynamicAllocation vb_alloc = cmd_list->AllocateTransient(vb_stride * vb_count, 16);

					vb_alloc.Update(transient_lines.data(), transient_vb_count * vb_stride, 0);
					vb_alloc.Update(persistent_lines.data(), persistent_vb_count * vb_stride, transient_vb_count * vb_stride);

					GfxVertexBufferView vbv[] = { GfxVertexBufferView(vb_alloc.gpu_address, vb_count, vb_stride) };

					cmd_list->SetPipelineState(debug_psos->Get<0>());
					cmd_list->SetVertexBuffers(vbv);
					cmd_list->SetTopology(GfxPrimitiveTopology::LineList);
					cmd_list->Draw(vb_count);
					transient_lines.clear();
				}
				if (triangles_count != 0)
				{
					uint32 transient_vb_count = transient_triangles_count * sizeof(DebugTriangle) / vb_stride;
					uint32 persistent_vb_count = persistent_triangles_count * sizeof(DebugTriangle) / vb_stride;
					uint32 vb_count = transient_vb_count + persistent_vb_count;
					GfxDynamicAllocation vb_alloc = cmd_list->AllocateTransient(vb_stride * vb_count, 16);

					vb_alloc.Update(transient_triangles.data(), transient_vb_count * vb_stride, 0);
					vb_alloc.Update(persistent_triangles.data(), persistent_vb_count * vb_stride, transient_vb_count * vb_stride);

					GfxVertexBufferView vbv[] = { GfxVertexBufferView(vb_alloc.gpu_address, vb_count, vb_stride) };

					cmd_list->SetPipelineState(debug_psos->Get<1>());
					cmd_list->SetVertexBuffers(vbv);
					cmd_list->SetTopology(GfxPrimitiveTopology::TriangleList);
					cmd_list->Draw(vb_count);
					transient_triangles.clear();
				}
				
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void DebugRenderer::AddLine(Vector3 const& start, Vector3 const& end, Color color)
	{
		std::vector<DebugLine>& lines = (mode == DebugRendererMode::Transient ? transient_lines : persistent_lines);
		lines.emplace_back(start, end, color);
	}

	void DebugRenderer::AddRay(Vector3 const& origin, Vector3 const& direction, Color color)
	{
		std::vector<DebugLine>& lines = (mode == DebugRendererMode::Transient ? transient_lines : persistent_lines);
		lines.emplace_back(origin, origin + direction, color);
	}

	void DebugRenderer::AddTriangle(Vector3 const& a, Vector3 const& b, Vector3 const& c, Color color, bool wireframe /*= false*/)
	{
		if (wireframe)
		{
			AddLine(a, b, color);
			AddLine(b, c, color);
			AddLine(c, b, color);
		}
		else
		{
			std::vector<DebugTriangle>& triangles = (mode == DebugRendererMode::Transient ? transient_triangles : persistent_triangles);
			triangles.emplace_back(a, b, c, color);
		}
	}

	void DebugRenderer::AddQuad(Vector3 const& a, Vector3 const& b, Vector3 const& c, Vector3 const& d, Color color, bool wireframe /*= false*/)
	{
		if (wireframe)
		{
			AddLine(a, b, color);
			AddLine(b, c, color);
			AddLine(c, d, color);
			AddLine(d, a, color);
		}
		else
		{
			AddTriangle(a, b, c, color, wireframe);
			AddTriangle(c, d, a, color, wireframe);
		}
	}

	void DebugRenderer::AddBox(Vector3 const& center, Vector3 const& extents, Color color, bool wireframe)
	{
		Vector3 min(center.x - extents.x, center.y - extents.y, center.z - extents.z);
		Vector3 max(center.x + extents.x, center.y + extents.y, center.z + extents.z);

		Vector3 v1(max.x, min.y, min.z);
		Vector3 v2(max.x, max.y, min.z);
		Vector3 v3(min.x, max.y, min.z);
		Vector3 v4(min.x, min.y, max.z);
		Vector3 v5(max.x, min.y, max.z);
		Vector3 v6(min.x, max.y, max.z);

		if (wireframe)
		{
			AddLine(min, v1, color);
			AddLine(v1, v2, color);
			AddLine(v2, v3, color);
			AddLine(v3, min, color);
			AddLine(v4, v5, color);
			AddLine(v5, max, color);
			AddLine(max, v6, color);
			AddLine(v6, v4, color);
			AddLine(min, v4, color);
			AddLine(v1, v5, color);
			AddLine(v2, max, color);
			AddLine(v3, v6, color);
		}
		else
		{
			AddQuad(v3, v2, v1, min, color);
			AddQuad(v4, v5, max, v6, color);
			AddQuad(min, v4, v6, v3, color);
			AddQuad(v2, max, v5, v1, color);
			AddQuad(v6, max, v2, v3, color);
			AddQuad(min, v1, v5, v4, color);
		}
		
	}

	void DebugRenderer::AddBoundingBox(BoundingBox const& bounding_box, Color color, bool wireframe)
	{
		AddBox(bounding_box.Center, bounding_box.Extents, color, wireframe);
	}

	void DebugRenderer::AddBoundingBox(BoundingBox const& bounding_box, Matrix const& transform, Color color, bool wireframe)
	{
		Vector3 min(bounding_box.Center.x - bounding_box.Extents.x, bounding_box.Center.y - bounding_box.Extents.y, bounding_box.Center.z - bounding_box.Extents.z);
		Vector3 max(bounding_box.Center.x + bounding_box.Extents.x, bounding_box.Center.y + bounding_box.Extents.y, bounding_box.Center.z + bounding_box.Extents.z);

		Vector3 v0(Vector3::Transform(min, transform));
		Vector3 v1(Vector3::Transform(Vector3(max.x, min.y, min.z), transform));
		Vector3 v2(Vector3::Transform(Vector3(max.x, max.y, min.z), transform));
		Vector3 v3(Vector3::Transform(Vector3(min.x, max.y, min.z), transform));
		Vector3 v4(Vector3::Transform(Vector3(min.x, min.y, max.z), transform));
		Vector3 v5(Vector3::Transform(Vector3(max.x, min.y, max.z), transform));
		Vector3 v6(Vector3::Transform(Vector3(min.x, max.y, max.z), transform));
		Vector3 v7(Vector3::Transform(max, transform));

		if (wireframe)
		{
			AddLine(v0, v1, color);
			AddLine(v1, v2, color);
			AddLine(v2, v3, color);
			AddLine(v3, v0, color);
			AddLine(v4, v5, color);
			AddLine(v5, v7, color);
			AddLine(v7, v6, color);
			AddLine(v6, v4, color);
			AddLine(v0, v4, color);
			AddLine(v1, v5, color);
			AddLine(v2, v7, color);
			AddLine(v3, v6, color);
		}
		else
		{
			AddQuad(v0, v1, v2, v3, color);
			AddQuad(v4, v5, v7, v6, color);
			AddQuad(v0, v4, v6, v3, color);
			AddQuad(v1, v5, v7, v2, color);
			AddQuad(v3, v2, v7, v6, color);
			AddQuad(v0, v1, v5, v4, color);
		}
	}

	void DebugRenderer::AddSphere(BoundingSphere const& sphere, Color color, bool wireframe)
	{
		AddSphere(sphere.Center, sphere.Radius, color, wireframe);
	}

	void DebugRenderer::AddSphere(Vector3 const& center, float radius, Color color, bool wireframe /*= true*/)
	{
		static constexpr uint32 SLICES = 16;
		static constexpr uint32 STACKS = 16;

		SpherePointHelper sphere_point_helper{ .center = center, .radius = radius };

		float j_step = pi<float> / SLICES;
		float i_step = pi<float> / STACKS;

		if (wireframe)
		{
			for (float j = 0; j < pi<float>; j += j_step)
			{
				for (float i = 0; i < pi<float> *2; i += i_step)
				{
					Vector3 p1 = sphere_point_helper(i, j);
					Vector3 p2 = sphere_point_helper(i + i_step, j);
					Vector3 p3 = sphere_point_helper(i, j + j_step);
					Vector3 p4 = sphere_point_helper(i + i_step, j + j_step);

					AddLine(p1, p2, color);
					AddLine(p3, p4, color);
					AddLine(p1, p3, color);
					AddLine(p2, p4, color);
				}
			}
		}
		else
		{
			for (float j = 0; j < pi<float>; j += j_step)
			{
				for (float i = 0; i < pi<float> *2; i += i_step)
				{
					Vector3 p1 = sphere_point_helper(i, j);
					Vector3 p2 = sphere_point_helper(i + i_step, j);
					Vector3 p3 = sphere_point_helper(i, j + j_step);
					Vector3 p4 = sphere_point_helper(i + i_step, j + j_step);

					AddQuad(p2, p1, p3, p4, color);
				}
			}
		}
	}

	void DebugRenderer::AddFrustum(BoundingFrustum const& frustum, Color color)
	{
		Vector3 corners[BoundingFrustum::CORNER_COUNT];
		frustum.GetCorners(corners);

		AddLine(corners[0], corners[1], color);
		AddLine(corners[1], corners[2], color);
		AddLine(corners[2], corners[3], color);
		AddLine(corners[3], corners[0], color);
		AddLine(corners[4], corners[5], color);
		AddLine(corners[5], corners[6], color);
		AddLine(corners[6], corners[7], color);
		AddLine(corners[7], corners[4], color);
		AddLine(corners[0], corners[4], color);
		AddLine(corners[1], corners[5], color);
		AddLine(corners[2], corners[6], color);
		AddLine(corners[3], corners[7], color);
	}

	void DebugRenderer::CreatePSOs()
	{
		using enum GfxShaderStage;
		GfxGraphicsPipelineStateDesc gfx_pso_desc = {};
		GfxReflection::FillInputLayoutDesc(GetGfxShader(VS_Debug), gfx_pso_desc.input_layout);
		gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
		gfx_pso_desc.VS = VS_Debug;
		gfx_pso_desc.PS = PS_Debug;
		gfx_pso_desc.num_render_targets = 1;
		gfx_pso_desc.rtv_formats[0] = GfxFormat::R8G8B8A8_UNORM;
		gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
		gfx_pso_desc.depth_state.depth_enable = true;
		gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
		gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
		gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
		gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Wireframe;
		gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Line;

		debug_psos = std::make_unique<GfxGraphicsPipelineStatePermutations>(2, gfx_pso_desc);
		debug_psos->SetTopologyType<1>(GfxPrimitiveTopologyType::Triangle);
		debug_psos->SetFillMode<1>(GfxFillMode::Solid);
		debug_psos->Finalize(gfx);
	}

	DebugRenderer::DebugRenderer() = default;
	DebugRenderer::~DebugRenderer() = default;
}

