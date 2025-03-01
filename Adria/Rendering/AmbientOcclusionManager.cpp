#include "AmbientOcclusionManager.h"
#include "Core/ConsoleManager.h"
#include "Graphics/GfxDevice.h"
#include "Editor/GUICommand.h"

namespace adria
{
	enum AmbientOcclusionType : Int
	{
		AmbientOcclusionType_None,
		AmbientOcclusionType_SSAO,
		AmbientOcclusionType_HBAO,
		AmbientOcclusionType_CACAO,
		AmbientOcclusionType_RTAO
	};

	static TAutoConsoleVariable<Int>  AmbientOcclusion("r.AmbientOcclusion", AmbientOcclusionType_SSAO, "0 - No AO, 1 - SSAO, 2 - HBAO, 3 - CACAO, 4 - RTAO");

	AmbientOcclusionManager::AmbientOcclusionManager(GfxDevice* gfx, Uint32 width, Uint32 height)
		: gfx(gfx), ssao_pass(gfx, width, height), hbao_pass(gfx, width, height), rtao_pass(gfx, width, height), cacao_pass(gfx, width, height)
	{
	}

	AmbientOcclusionManager::~AmbientOcclusionManager()
	{
	}

	void AmbientOcclusionManager::OnResize(Uint32 w, Uint32 h)
	{
		ssao_pass.OnResize(w, h);
		hbao_pass.OnResize(w, h);
		cacao_pass.OnResize(w, h);
		rtao_pass.OnResize(w, h);
	}

	void AmbientOcclusionManager::OnSceneInitialized()
	{
		ssao_pass.OnSceneInitialized();
		hbao_pass.OnSceneInitialized();
	}

	void AmbientOcclusionManager::AddPass(RenderGraph& rg)
	{
		switch (AmbientOcclusion.Get())
		{
		case AmbientOcclusionType_SSAO:  ssao_pass.AddPass(rg); break;
		case AmbientOcclusionType_HBAO:  hbao_pass.AddPass(rg); break;
		case AmbientOcclusionType_CACAO: cacao_pass.AddPass(rg); break;
		case AmbientOcclusionType_RTAO:  rtao_pass.AddPass(rg); break;
		}
	}

	void AmbientOcclusionManager::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::Combo("Ambient Occlusion Type", AmbientOcclusion.GetPtr(), "None\0SSAO\0HBAO\0CACAO\0RTAO\0", 5))
				{
					if (!gfx->GetCapabilities().SupportsRayTracing() && AmbientOcclusion.Get() == 4) AmbientOcclusion->Set(AmbientOcclusionType_SSAO);
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_AO);
		switch (AmbientOcclusion.Get())
		{
		case AmbientOcclusionType_SSAO:  ssao_pass.GUI();  break;
		case AmbientOcclusionType_HBAO:  hbao_pass.GUI();  break;
		case AmbientOcclusionType_CACAO: cacao_pass.GUI(); break;
		case AmbientOcclusionType_RTAO:  rtao_pass.GUI();  break;
		}
	}

}
