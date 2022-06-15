#pragma once
#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "../Graphics/Buffer.h"


namespace adria
{
	struct PickingData
	{
		DirectX::XMFLOAT4 position;
		DirectX::XMFLOAT4 normal;
	};

	class GraphicsDevice;
	class RenderGraph;

	class PickingPass
	{
	public:
		
		PickingPass(GraphicsDevice* gfx, uint32 width, uint32 height);

		void OnResize(uint32 w, uint32 h);

		void AddPass(RenderGraph& rg);

		PickingData GetPickingData() const;

	private:
		GraphicsDevice* gfx;
		std::vector<std::unique_ptr<Buffer>> read_picking_buffers;
		uint32 width, height;
	};
}