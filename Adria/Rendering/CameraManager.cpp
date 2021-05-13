#include "CameraManager.h"
#include "../Input/Input.h"
#include "../Logging/Logger.h"
#include "../Core/Macros.h"

namespace adria
{
	CameraManager::CameraManager(Input& input) : input{ input }
	{
	}

	void CameraManager::AddCamera(camera_desc_t const& camera_desc)
	{
		cameras.emplace_back(camera_desc);
	}
	void CameraManager::Update(f32 dt)
	{
		
		if (cameras.empty() || !update_camera) return;

		auto& camera = cameras[current_camera];

		if (input.GetKey(KeyCode::Space)) return;

		float speed_factor = 1.0f;

		if (input.GetKey(KeyCode::ShiftLeft)) speed_factor *= 5.0f;
		if (input.GetKey(KeyCode::CtrlLeft))  speed_factor *= 0.2f;

		if (input.GetKey(KeyCode::W))
			camera.Walk(speed_factor * dt);

		if (input.GetKey(KeyCode::S))
			camera.Walk(-speed_factor * dt);

		if (input.GetKey(KeyCode::A))
			camera.Strafe(-speed_factor * dt);

		if (input.GetKey(KeyCode::D))
			camera.Strafe(speed_factor * dt);

		f32 dx = input.GetMouseDeltaX();
		f32 dy = input.GetMouseDeltaY();

		camera.Pitch((i64)dy);
		camera.Yaw((i64)dx);

		camera.UpdateViewMatrix();
	}
	Camera const& CameraManager::GetActiveCamera() const
	{
		return cameras[current_camera];
	}
	Camera& CameraManager::GetActiveCamera() 
	{
		return cameras[current_camera];
	}
	void CameraManager::OnResize(u32 width, u32 height)
	{
		for (auto& camera : cameras)
			camera.OnResize(width, height);

	}
	void CameraManager::OnScroll(i32 scroll)
	{
		if (update_camera)
		{
			auto& camera = cameras[current_camera];
			camera.Zoom((f32)scroll);
		}
	}
	void CameraManager::ShouldUpdate(bool update)
	{
		update_camera = update;
	}
}