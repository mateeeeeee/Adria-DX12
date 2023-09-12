#pragma once
#include <vector>
#include <DirectXCollision.h>
#include "Utilities/Singleton.h"

using namespace DirectX;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	//#todo add shaders and psos to cache
	class DebugRenderer : public Singleton<DebugRenderer>
	{
		friend class Singleton<DebugRenderer>;

		struct DebugLine
		{
			XMFLOAT3 start;
			XMFLOAT3 end;
		};

	public:
		void Initialize(GfxDevice* gfx);
		void Destroy();

		void Render(RenderGraph& rg);

		void RenderLine(XMFLOAT3 const& start, XMFLOAT3 const& end, Color col);
		void RenderRay(XMFLOAT3 const& origin, XMFLOAT3 const& direction, Color col);
		void RenderBox(XMFLOAT3 const& center, XMFLOAT3 const& extents, Color col);
		void RenderBoundingBox(BoundingBox const& bounding_box, Color col);
		void RenderBoundingBox(BoundingBox const& bounding_box, XMFLOAT4X4 transform, Color col);
		void RenderSphere(BoundingSphere const& sphere, Color col);
		void RenderFrustum(BoundingFrustum const& frustum, Color col);

	private:
		GfxDevice* gfx;
		std::vector<DebugLine> lines;

	private:
		DebugRenderer() = default;
		~DebugRenderer() = default;
	};
	#define g_DebugRenderer DebugRenderer::Get()
}