#pragma once
#include "Camera.h"
#include <unordered_map>
#include <string>

namespace adria
{
	class Input;

	
	class CameraManager
	{
	public:
		CameraManager(Input&);

		void AddCamera(camera_parameters_t const& camera_desc);

		void Update(float32 dt);

		Camera const& GetActiveCamera() const;

		Camera& GetActiveCamera();

		void OnResize(uint32 width, uint32 height);

		void OnScroll(int32 scroll);

		void ShouldUpdate(bool update);

	private:
		Input& input;
		std::vector<Camera> cameras;
		uint32 current_camera = 0;
		bool update_camera = true;
	};
}