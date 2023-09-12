#include "DebugRenderer.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{



	void DebugRenderer::Initialize(GfxDevice* _gfx)
	{
		gfx = _gfx;
	}

	void DebugRenderer::Destroy()
	{
		gfx = nullptr;
	}

	void DebugRenderer::Render(RenderGraph& rg)
	{
		if (lines.empty()) return;

		rg.AddPass<void>("Debug Renderer Pass",
			[=](RenderGraphBuilder& builder)
			{
				
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				
			}, RGPassType::Graphics, RGPassFlags::None);

		lines.clear();
	}

	void DebugRenderer::RenderLine(XMFLOAT3 const& start, XMFLOAT3 const& end, Color col /*= Color::Red()*/)
	{
		lines.emplace_back(start, end);
	}

	void DebugRenderer::RenderRay(XMFLOAT3 const& origin, XMFLOAT3 const& direction, Color col /*= Color::Red()*/)
	{
		XMFLOAT3 end;
		XMStoreFloat3(&end, XMVectorAdd(XMLoadFloat3(&origin), XMLoadFloat3(&direction)));
		lines.emplace_back(origin, end);
	}

	void DebugRenderer::RenderBox(XMFLOAT3 const& center, XMFLOAT3 const& extents, Color color /*= Color::Red()*/)
	{
		XMFLOAT3 min(center.x - extents.x, center.y - extents.y, center.z - extents.z);
		XMFLOAT3 max(center.x + extents.x, center.y + extents.y, center.z + extents.z);

		XMFLOAT3 v1(max.x, min.y, min.z);
		XMFLOAT3 v2(max.x, max.y, min.z);
		XMFLOAT3 v3(min.x, max.y, min.z);
		XMFLOAT3 v4(min.x, min.y, max.z);
		XMFLOAT3 v5(max.x, min.y, max.z);
		XMFLOAT3 v6(min.x, max.y, max.z);

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

	void DebugRenderer::RenderBoundingBox(BoundingBox const& bounding_box, Color col /*= Color::Red()*/)
	{
		RenderBox(bounding_box.Center, bounding_box.Extents, col);
	}

	void DebugRenderer::RenderBoundingBox(BoundingBox const& bounding_box, XMFLOAT4X4 transform, Color col /*= Color::Red()*/)
	{

	}

	void DebugRenderer::RenderSphere(BoundingSphere const& sphere, Color col /*= Color::Red()*/)
	{

	}

	void DebugRenderer::RenderFrustum(BoundingFrustum const& frustum, Color color /*= Color::Red()*/)
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

