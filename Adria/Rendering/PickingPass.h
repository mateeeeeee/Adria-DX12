#pragma once


namespace adria
{
	struct PickingData
	{
		Vector4 position;
		Vector4 normal;
	};

	class GfxDevice;
	class GfxBuffer;
	class GfxComputePipelineState;
	class RenderGraph;

	class PickingPass
	{
	public:
		
		PickingPass(GfxDevice* gfx, Uint32 width, Uint32 height);

		void OnResize(Uint32 w, Uint32 h);

		void AddPass(RenderGraph& rg);

		PickingData GetPickingData() const;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::vector<std::unique_ptr<GfxBuffer>> read_picking_buffers;
		std::unique_ptr<GfxComputePipelineState> picking_pso;

	private:
		void CreatePSO();
		void CreatePickingBuffers();
	};
}