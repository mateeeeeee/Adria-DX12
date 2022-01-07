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

		void Update(F32 dt);

		Camera const& GetActiveCamera() const;

		Camera& GetActiveCamera();

		void OnResize(U32 width, U32 height);

		void OnScroll(I32 scroll);

		void ShouldUpdate(bool update);

	private:
		Input& input;
		std::vector<Camera> cameras;
		U32 current_camera = 0;
		bool update_camera = true;
	};
}