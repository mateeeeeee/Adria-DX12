#pragma once
#include "RendererSettings.h"
#include "SceneViewport.h"
#include "ConstantBuffers.h"
#include "Passes/GBufferPass.h"
#include "Passes/AmbientPass.h"
#include "../Graphics/ShaderUtility.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/GPUProfiler.h"
#include "../Utilities/CPUProfiler.h" 
#include "../RenderGraph/RenderGraphResourcePool.h"



namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class Camera;
	class Buffer;
	class Texture;
	struct Light;

	class RenderGraphRenderer
	{
		enum ENullHeapSlot
		{
			NULL_HEAP_SLOT_TEXTURE2D,
			NULL_HEAP_SLOT_TEXTURECUBE,
			NULL_HEAP_SLOT_TEXTURE2DARRAY,
			NULL_HEAP_SLOT_RWTEXTURE2D,
			NULL_HEAP_SIZE
		};

	public:

		RenderGraphRenderer(tecs::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);

		void NewFrame(Camera const* camera);
		void Update(float32 dt);
		void Render(RendererSettings const&);

		void SetProfilerSettings(ProfilerSettings const&);
		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();

		TextureManager& GetTextureManager();
	private:
		tecs::registry& reg;
		GraphicsDevice* gfx;
		RGResourcePool resource_pool;

		TextureManager texture_manager;
		Camera const* camera;
		CPUProfiler cpu_profiler;
		GPUProfiler gpu_profiler;

		RendererSettings settings;
		ProfilerSettings profiler_settings;

		uint32 const backbuffer_count;
		uint32 backbuffer_index;
		uint32 width;
		uint32 height;


		std::unique_ptr<DescriptorHeap> null_heap;
		//Persistent constant buffers
		ConstantBuffer<FrameCBuffer> frame_cbuffer;

		GBufferPass gbuffer_pass;
		AmbientPass ambient_pass;
	private:
		void CreateNullHeap();
		void UpdatePersistentConstantBuffers(float32 dt);
	};
}