#include "DebugRenderer.h"
#include "BlackboardData.h"
#include "PSOCache.h"
#include "Graphics/GfxDynamicAllocation.h"
#include "RenderGraph/RenderGraph.h"
#include "Math/Packing.h"

namespace adria
{
	uint32 DebugRenderer::ColorToUint(Color const& col)
	{
		return PackToUint(col.R(), col.G(), col.B(), col.A());
	}

	void DebugRenderer::Initialize(uint32 _width, uint32 _height)
	{
		width = _width;
		height = _height;
	}

	void DebugRenderer::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void DebugRenderer::Render(RenderGraph& rg)
	{
		if (lines.empty()) return;
		FrameBlackboardData const& global_data = rg.GetBlackboard().Get<FrameBlackboardData>();


		rg.AddPass<void>("Debug Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.WriteDepthStencil(RG_RES_NAME(DepthStencil), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				cmd_list->SetPipelineState(
						PSOCache::Get(GfxPipelineStateID::Debug_Wireframe));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);

				//#todo double check this 
				constexpr uint32 vb_stride = (uint32)sizeof(DebugLine) / 2;
				uint32 line_count = (uint32)lines.size();
				GfxDynamicAllocation vb_alloc = cmd_list->AllocateTransient(vb_stride * line_count, 16);
				vb_alloc.Update(lines.data(), line_count * sizeof(DebugLine));
				GfxVertexBufferView vbv[] = { GfxVertexBufferView(vb_alloc.gpu_address, line_count, vb_stride) };
				cmd_list->SetVertexBuffers(vbv, 0);

			}, RGPassType::Graphics, RGPassFlags::None);

		lines.clear();
	}

	void DebugRenderer::RenderLine(Vector3 const& start, Vector3 const& end, Color col)
	{
		lines.emplace_back(start, end, col);
	}

	void DebugRenderer::RenderRay(Vector3 const& origin, Vector3 const& direction, Color col)
	{
		lines.emplace_back(origin, origin + direction, col);
	}

	void DebugRenderer::RenderBox(Vector3 const& center, Vector3 const& extents, Color color)
	{
		Vector3 min(center.x - extents.x, center.y - extents.y, center.z - extents.z);
		Vector3 max(center.x + extents.x, center.y + extents.y, center.z + extents.z);

		Vector3 v1(max.x, min.y, min.z);
		Vector3 v2(max.x, max.y, min.z);
		Vector3 v3(min.x, max.y, min.z);
		Vector3 v4(min.x, min.y, max.z);
		Vector3 v5(max.x, min.y, max.z);
		Vector3 v6(min.x, max.y, max.z);

		RenderLine(min, v1, color);
		RenderLine(v1, v2, color);
		RenderLine(v2, v3, color);
		RenderLine(v3, min, color);
		RenderLine(v4, v5, color);
		RenderLine(v5, max, color);
		RenderLine(max, v6, color);
		RenderLine(v6, v4, color);
		RenderLine(min, v4, color);
		RenderLine(v1, v5, color);
		RenderLine(v2, max, color);
		RenderLine(v3, v6, color);
	}

	void DebugRenderer::RenderBoundingBox(BoundingBox const& bounding_box, Color col)
	{
		RenderBox(bounding_box.Center, bounding_box.Extents, col);
	}

	void DebugRenderer::RenderBoundingBox(BoundingBox const& bounding_box, Matrix const& transform, Color col)
	{

	}

	void DebugRenderer::RenderSphere(BoundingSphere const& sphere, Color col)
	{

	}

	void DebugRenderer::RenderFrustum(BoundingFrustum const& frustum, Color color)
	{
		std::vector<XMFLOAT3> corners(BoundingFrustum::CORNER_COUNT);
		frustum.GetCorners(corners.data());

		RenderLine(corners[0], corners[1], color);
		RenderLine(corners[1], corners[2], color);
		RenderLine(corners[2], corners[3], color);
		RenderLine(corners[3], corners[0], color);
		RenderLine(corners[4], corners[5], color);
		RenderLine(corners[5], corners[6], color);
		RenderLine(corners[6], corners[7], color);
		RenderLine(corners[7], corners[4], color);
		RenderLine(corners[0], corners[4], color);
		RenderLine(corners[1], corners[5], color);
		RenderLine(corners[2], corners[6], color);
		RenderLine(corners[3], corners[7], color);
	}

}

