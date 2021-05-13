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

		void AddCamera(camera_desc_t const& camera_desc);

		void Update(f32 dt);

		Camera const& GetActiveCamera() const;

		Camera& GetActiveCamera();

		void OnResize(u32 width, u32 height);

		void OnScroll(i32 scroll);

		void ShouldUpdate(bool update);

	private:
		Input& input;
		std::vector<Camera> cameras;
		u32 current_camera = 0;
		bool update_camera = true;
	};
}