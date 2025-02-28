#include "SceneConfig.h"
#include "Core/Paths.h"
#include "Math/Constants.h"
#include "Utilities/JsonUtil.h"
#include "Utilities/FilesUtil.h"

using json = nlohmann::json;
using namespace DirectX;

namespace adria
{
	Bool ParseSceneConfig(std::string const& scene_file, SceneConfig& config, Bool append_dir)
	{
		json models, lights, camera, skybox;
		std::string ini_file;
		try
		{
			std::string scene_file_full = append_dir ? paths::ScenesDir + scene_file : scene_file;
			JsonParams scene_params = json::parse(std::ifstream(scene_file_full));
			models = scene_params.FindJsonArray("models");
			lights = scene_params.FindJsonArray("lights");
			camera = scene_params.FindJson("camera");
			skybox = scene_params.FindJson("skybox");
			ini_file = scene_params.FindOr<std::string>("ini", "default_cvars.ini");
		}
		catch (json::parse_error const& e)
		{
			ADRIA_LOG(ERROR, "Scene json parsing error: %s! ", e.what());
			return false;
		}

		for (auto&& model_json : models)
		{
			JsonParams model_params(model_json);

			std::string path;
			if (!model_params.Find<std::string>("path", path))
			{
				ADRIA_LOG(WARNING, "Model doesn't have path field! Skipping this model...");
			}
			std::string tex_path = model_params.FindOr<std::string>("tex_path", GetParentPath(path) + "\\");

			Float position[3] = { 0.0f, 0.0f, 0.0f };
			model_params.FindArray("translation", position);
			Matrix translation = XMMatrixTranslation(position[0], position[1], position[2]);

			Float angles[3] = { 0.0f, 0.0f, 0.0f };
			model_params.FindArray("rotation", angles);
			std::transform(std::begin(angles), std::end(angles), std::begin(angles), XMConvertToRadians);
			Matrix rotation = XMMatrixRotationX(angles[0]) * XMMatrixRotationY(angles[1]) * XMMatrixRotationZ(angles[2]);

			Float scale_factors[3] = { 1.0f, 1.0f, 1.0f };
			model_params.FindArray("scale", scale_factors);
			Matrix scale = XMMatrixScaling(scale_factors[0], scale_factors[1], scale_factors[2]);
			Matrix transform = rotation * scale * translation;

			Bool triangle_ccw = true;
			model_params.Find<Bool>("use_ccw", triangle_ccw);
			Bool force_mask = false;
			model_params.Find<Bool>("force_alpha_mask", force_mask);
			Bool load_model_lights = false;
			model_params.Find<Bool>("load_model_lights", load_model_lights);
			config.scene_models.emplace_back(path, tex_path, transform, triangle_ccw, force_mask, load_model_lights);
		}

		for (auto&& light_json : lights)
		{
			JsonParams light_params(light_json);

			std::string type;
			if (!light_params.Find<std::string>("type", type))
			{
				ADRIA_LOG(WARNING, "Light doesn't have type field! Skipping this light...");
			}

			LightParameters light{};
			Float position[3] = { 0.0f, 0.0f, 0.0f };
			light_params.FindArray("position", position);
			light.light_data.position = Vector4(position[0], position[1], position[2], 1.0f);

			Float direction[3] = { 0.0f, -1.0f, 0.0f };
			if (light_params.FindArray("direction", direction))
			{
				light.light_data.direction = Vector4(direction[0], direction[1], direction[2], 0.0f);
			}
			else if (type == "directional")
			{
				Float elevation, azimuth;
				if (light_params.Find<Float>("elevation", elevation) && light_params.Find<Float>("azimuth", azimuth))
				{
					light.light_data.direction = -ConvertElevationAndAzimuthToDirection(elevation, azimuth);
				}
			}

			Float color[3] = { 1.0f, 1.0f, 1.0f };
			if (light_params.FindArray("color", color))
			{
				light.light_data.color = Vector4(color[0], color[1], color[2], 1.0f);
			}
			else if (type == "directional")
			{
				Float temperature;
				if (light_params.Find<Float>("temperature", temperature))
				{
					light.light_data.color = ConvertTemperatureToColor(temperature);
				}
			}

			light.light_data.intensity = light_params.FindOr<Float>("intensity", 1.0f);
			light.light_data.range = light_params.FindOr<Float>("range", 100.0f);

			light.light_data.outer_cosine = std::cos(XMConvertToRadians(light_params.FindOr<Float>("outer_angle", 45.0f)));
			light.light_data.inner_cosine = std::cos(XMConvertToRadians(light_params.FindOr<Float>("outer_angle", 22.5f)));

			light.light_data.casts_shadows = light_params.FindOr<Bool>("shadows", true);
			light.light_data.use_cascades = light_params.FindOr<Bool>("cascades", false);
			light.light_data.ray_traced_shadows = light_params.FindOr<Bool>("rts", false);

			light.light_data.active = light_params.FindOr<Bool>("active", true);
			light.light_data.volumetric = light_params.FindOr<Bool>("volumetric", false);
			light.light_data.volumetric_strength = light_params.FindOr<Float>("volumetric_strength", 0.004f);

			light.light_data.lens_flare = light_params.FindOr<Bool>("lens_flare", false);
			light.light_data.god_rays = light_params.FindOr<Bool>("god_rays", false);

			light.light_data.godrays_decay = light_params.FindOr<Float>("godrays_decay", 0.825f);
			light.light_data.godrays_exposure = light_params.FindOr<Float>("godrays_exposure", 2.0f);
			light.light_data.godrays_density = light_params.FindOr<Float>("godrays_density", 0.975f);
			light.light_data.godrays_weight = light_params.FindOr<Float>("godrays_weight", 0.25f);

			light.mesh_type = LightMesh::NoMesh;
			std::string mesh = light_params.FindOr<std::string>("mesh", "");
			if (mesh == "sphere")
			{
				light.mesh_type = LightMesh::Sphere;
			}
			else if (mesh == "quad")
			{
				light.mesh_type = LightMesh::Quad;
			}
			light.mesh_size = light_params.FindOr<Uint32>("size", 100u);
			light.light_texture = light_params.FindOr<std::string>("texture", "");
			if (light.light_texture.has_value() && light.light_texture->empty()) light.light_texture = std::nullopt;

			if (type == "directional")
			{
				light.light_data.type = LightType::Directional;
			}
			else if (type == "point")
			{
				light.light_data.type = LightType::Point;
			}
			else if (type == "spot")
			{
				light.light_data.type = LightType::Spot;
			}
			else
			{
				ADRIA_LOG(WARNING, "Light has invalid type %s! Skipping this light...", type.c_str());
			}

			config.scene_lights.push_back(std::move(light));
		}

		JsonParams camera_params(camera);
		config.camera_params.near_plane = camera_params.FindOr<Float>("near", 1.0f);
		config.camera_params.far_plane = camera_params.FindOr<Float>("far", 3000.0f);
		config.camera_params.fov = XMConvertToRadians(camera_params.FindOr<Float>("fov", 90.0f));

		Float position[3] = { 0.0f, 0.0f, 0.0f };
		camera_params.FindArray("position", position);
		config.camera_params.position = Vector3(position);

		Float look_at[3] = { 0.0f, 0.0f, 10.0f };
		camera_params.FindArray("look_at", look_at);
		config.camera_params.look_at = Vector3(look_at);

		JsonParams skybox_params(skybox);
		std::vector<std::string> skybox_textures;

		std::string cubemap[1];
		if (skybox_params.FindArray("texture", cubemap))
		{
			config.skybox_params.cubemap = cubemap[0];
		}
		else
		{
			std::string cubemap[6];
			if (skybox_params.FindArray("texture", cubemap))
			{
				config.skybox_params.cubemap_textures = std::to_array(cubemap);
			}
			else
			{
				ADRIA_LOG(WARNING, "Skybox texture not found or is incorrectly specified!  \
								Size of texture array has to be either 1 or 6! Fallback to the default one...");
				config.skybox_params.cubemap = paths::TexturesDir + "Skybox/sunsetcube1024.dds";
			}
		}
		config.ini_file = ini_file;

		return true;
	}

}

