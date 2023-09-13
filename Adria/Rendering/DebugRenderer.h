#pragma once
#include <vector>
#include <DirectXColors.h>
#include "Utilities/Singleton.h"

using namespace DirectX;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class DebugRenderer : public Singleton<DebugRenderer>
	{
		friend class Singleton<DebugRenderer>;

		static uint32 ColorToUint(Color const& col);
		struct DebugLine
		{
			DebugLine() {}
			DebugLine(Vector3 const& start, Vector3 const& end, Color const& color)
				: start(start), color_start(ColorToUint(color)), end(end), color_end(color_start)
			{}
			DebugLine(Vector3 const& start, Vector3 const& end, Color const& color_start, Color const& color_end)
				: start(start), color_start(ColorToUint(color_start)), end(end), color_end(ColorToUint(color_end))
			{}
			Vector3 start;
			uint32 color_start = 0xffffffff;
			Vector3 end;
			uint32 color_end = 0xffffffff;
		};

	public:
		void Initialize(uint32 width, uint32 height);
		void OnResize(uint32 w, uint32 h);

		void Render(RenderGraph& rg);

		void RenderLine(Vector3 const& start, Vector3 const& end, Color col);
		void RenderRay(Vector3 const& origin, Vector3 const& direction, Color col);
		void RenderBox(Vector3 const& center, Vector3 const& extents, Color col);
		void RenderBoundingBox(BoundingBox const& bounding_box, Color col);
		void RenderBoundingBox(BoundingBox const& bounding_box, Matrix const& transform, Color col);
		void RenderSphere(BoundingSphere const& sphere, Color col);
		void RenderFrustum(BoundingFrustum const& frustum, Color col);

	private:
		std::vector<DebugLine> lines;
		uint32 width, height;

	private:
		DebugRenderer() = default;
		~DebugRenderer() = default;
	};
	#define g_DebugRenderer DebugRenderer::Get()
}