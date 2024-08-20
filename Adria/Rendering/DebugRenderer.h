#pragma once
#include "Graphics/GfxPipelineStatePermutationsFwd.h"
#include "Utilities/Singleton.h"

using namespace DirectX;

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	enum class DebugRendererMode : bool
	{
		Transient,
		Persistent
	};

	class DebugRenderer : public Singleton<DebugRenderer>
	{
		friend class Singleton<DebugRenderer>;

		static uint32 ColorToUint(Color const& col);

		struct DebugVertex
		{
			Vector3 position = Vector3(0,0,0);
			uint32 color = 0xffffffff;
		};
		struct DebugLine
		{
			DebugLine() {}
			DebugLine(Vector3 const& start, Vector3 const& end, Color const& color)
				: vertex_start{ start, ColorToUint(color) }, vertex_end{ end, ColorToUint(color) }
			{}
			DebugLine(Vector3 const& start, Vector3 const& end, Color const& color_start, Color const& color_end)
				: vertex_start{ start, ColorToUint(color_start) }, vertex_end{ end, ColorToUint(color_end) }
			{}
			DebugVertex vertex_start;
			DebugVertex vertex_end;
		};
		struct DebugTriangle
		{
			DebugTriangle() {}
			DebugTriangle(Vector3 const& v0, Vector3 const& v1, Vector3 const& v2, Color const& color)
				: vertex0{ v0, ColorToUint(color) }, vertex1{ v1, ColorToUint(color) }, vertex2{ v2, ColorToUint(color) }
			{}
			DebugTriangle(Vector3 const& v0, Vector3 const& v1, Vector3 const& v2
				, Color const& color0, Color const& color1, Color const& color2)
				: vertex0{ v0, ColorToUint(color0) }, vertex1{ v1, ColorToUint(color1) }, vertex2{ v2, ColorToUint(color2) }
			{}

			DebugVertex vertex0;
			DebugVertex vertex1;
			DebugVertex vertex2;
		};
	public:
		void Initialize(GfxDevice* gfx, uint32 width, uint32 height);
		void Destroy();
		void OnResize(uint32 w, uint32 h);

		void Render(RenderGraph& rg);

		void AddLine(Vector3 const& start, Vector3 const& end, Color color);
		void AddRay(Vector3 const& origin, Vector3 const& direction, Color color);
		void AddTriangle(Vector3 const& a, Vector3 const& b, Vector3 const& c, Color color, bool wireframe = false);
		void AddQuad(Vector3 const& a, Vector3 const& b, Vector3 const& c, Vector3 const& d, Color color, bool wireframe = false);
		void AddBox(Vector3 const& center, Vector3 const& extents, Color color, bool wireframe = true);
		void AddBoundingBox(BoundingBox const& bounding_box, Color color, bool wireframe = true);
		void AddBoundingBox(BoundingBox const& bounding_box, Matrix const& transform, Color color, bool wireframe = true);
		void AddSphere(Vector3 const& center, float radius, Color color, bool wireframe = true);
		void AddSphere(BoundingSphere const& sphere, Color color, bool wireframe = true);
		void AddFrustum(BoundingFrustum const& frustum, Color color);

		void SetMode(DebugRendererMode _mode) { mode = _mode; }
		void ClearPersistentLines() { persistent_lines.clear(); }
		void ClearPersistentTriangles() { persistent_triangles.clear(); }
		void ClearPersistent() { ClearPersistentLines(); ClearPersistentTriangles(); }

	private:
		GfxDevice* gfx;
		DebugRendererMode mode = DebugRendererMode::Transient;
		std::vector<DebugLine> transient_lines, persistent_lines;
		std::vector<DebugTriangle> transient_triangles, persistent_triangles;
		uint32 width = 0, height = 0;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> debug_psos;

	private:
		DebugRenderer();
		~DebugRenderer();

		void CreatePSOs();
	};
	#define g_DebugRenderer DebugRenderer::Get()
}