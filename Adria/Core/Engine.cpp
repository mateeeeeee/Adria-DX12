#include "../Tasks/TaskSystem.h"
#include "Engine.h"
#include "Window.h"
#include "Events.h"
#include "../Math/Constants.h"
#include "../Logging/Logger.h"
#include "../Graphics/GraphicsCoreDX12.h"
#include "../Rendering/Renderer.h"
#include "../Rendering/EntityLoader.h"
#include "../Utilities/Random.h"
#include "../Utilities/Timer.h"
#include "../Audio/AudioSystem.h"

namespace adria
{
	using namespace tecs;

	Engine::Engine(engine_init_t const& init)  : vsync{ init.vsync }, event_queue {}, input{ event_queue }, camera_manager{ input }
	{
		TaskSystem::Initialize();
		ShaderUtility::Initialize();

		gfx = std::make_unique<GraphicsCoreDX12>(Window::Handle());
		renderer = std::make_unique<Renderer>(reg, gfx.get(), Window::Width(), Window::Height());
		entity_loader = std::make_unique<EntityLoader>(reg, gfx.get(), renderer->GetTextureManager());
		
		event_queue.Subscribe<ResizeEvent>([this](ResizeEvent const& e) 
			{
				camera_manager.OnResize(e.width, e.height);
				gfx->ResizeBackbuffer(e.width, e.height);
				renderer->OnResize(e.width, e.height);
			});
		event_queue.Subscribe<ScrollEvent>([this](ScrollEvent const& e)
			{
				camera_manager.OnScroll(e.scroll);
			});

		if (init.load_default_scene) InitializeScene();
	}

	Engine::~Engine()
	{
		ShaderUtility::Destroy();
		TaskSystem::Destroy();
	}

	void Engine::HandleWindowMessage(window_message_t const& msg_data)
	{
		input.HandleWindowMessage(msg_data);
	}

	void Engine::Run(RendererSettings const& settings, bool offscreen)
	{

		static EngineTimer timer;

		float32 const dt = timer.MarkInSeconds();

		if (Window::IsActive() || true) //for now
		{
			input.NewFrame();
			event_queue.ProcessEvents();

			Update(dt);
			Render(settings, offscreen);
		}
		else
		{
			input.NewFrame();
			event_queue.ProcessEvents();
		}

	}

	void Engine::Update(float32 dt)
	{
		camera_manager.Update(dt);
		auto const& camera = camera_manager.GetActiveCamera();
		renderer->NewFrame(&camera);
		renderer->Update(dt);
	}

	void Engine::Render(RendererSettings const& settings, bool offscreen)
	{
		gfx->ClearBackbuffer();
		renderer->Render_Multithreaded(settings);

		if (offscreen) renderer->ResolveToOffscreenFramebuffer();
		else renderer->ResolveToBackbuffer();
	}

	void Engine::Present()
	{
		gfx->SwapBuffers(vsync);
	}

	void Engine::InitializeScene() 
	{
		gfx->ResetDefaultCommandList();

		camera_desc_t camera_desc{};
		camera_desc.aspect_ratio = static_cast<float32>(Window::Width()) / Window::Height();
		camera_desc.near_plane = 1.0f;
		camera_desc.far_plane = 1500.0f;
		camera_desc.fov = pi_div_4<float32>;
		camera_desc.position_x = 0.0f;
		camera_desc.position_y = 50.0f;
		camera_desc.position_z = 0.0f;
		camera_manager.AddCamera(camera_desc);
		
		skybox_parameters_t skybox_params{};
		skybox_params.cubemap_textures =
		{
			L"Resources/Textures/Skybox/right.jpg",
			L"Resources/Textures/Skybox/left.jpg",
			L"Resources/Textures/Skybox/top.jpg",
			L"Resources/Textures/Skybox/bottom.jpg",
			L"Resources/Textures/Skybox/front.jpg",
			L"Resources/Textures/Skybox/back.jpg"
		};
		entity_loader->LoadSkybox(skybox_params);

		model_parameters_t model_params{};
		model_params.model_path = "Resources/Models/Sponza/glTF/Sponza.gltf";
		model_params.textures_path = "Resources/Models/Sponza/glTF/";
		model_params.model_scale = 0.3f;
		entity_loader->LoadGLTFModel(model_params);

		//model_params.model_path = "Resources/Models/DamagedHelmet/glTF/DamagedHelmet.gltf";
		//model_params.textures_path = "Resources/Models/DamagedHelmet/glTF/";
		//model_params.model_scale = 3.0f;
		//model_params.merge_meshes = true;
		//entity_loader->LoadModel(model_params);
		
		light_parameters_t light_params{};
		light_params.light_data.casts_shadows = true;
		light_params.light_data.color = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		light_params.light_data.energy = 8.0f;
		light_params.light_data.direction = DirectX::XMVectorSet(0.1f, -1.0f, 0.25f, 0.0f);
		light_params.light_data.type = ELightType::Directional;
		light_params.light_data.active = true;
		light_params.light_data.use_cascades = true;
		light_params.light_data.volumetric = false;
		light_params.light_data.volumetric_strength = 1.0f;
		light_params.mesh_type = ELightMesh::Quad;
		light_params.mesh_size = 250;
		light_params.light_texture = L"Resources/Textures/sun.png";
		
		entity_loader->LoadLight(light_params);

		/*grid_parameters_t ocean_params{};
		ocean_params.tile_count_x = 50;
		ocean_params.tile_count_z = 50;
		ocean_params.tile_size_x = 10;
		ocean_params.tile_size_z = 10;
		ocean_params.texture_scale_x = 1;
		ocean_params.texture_scale_z = 1;

		ocean_parameters_t params{};
		params.ocean_grid = std::move(ocean_params);
		entity_loader->LoadOcean(params);*/

		renderer->GetTextureManager().GenerateAllMips();
		renderer->UploadData();

		gfx->ExecuteDefaultCommandList();
		gfx->WaitForGPU();
	}
}