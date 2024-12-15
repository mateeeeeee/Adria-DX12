#pragma once
#include "SceneLoader.h"
#include "Camera.h"

namespace adria
{
	struct SceneConfig
	{
		std::vector<ModelParameters> scene_models;
		std::vector<LightParameters> scene_lights;
		SkyboxParameters skybox_params;
		CameraParameters camera_params;
		std::string		 ini_file;
	};

	Bool ParseSceneConfig(std::string const& scene_file, SceneConfig& scene_config, Bool append_dir = true);
}