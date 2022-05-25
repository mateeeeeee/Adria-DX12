#include "CameraManager.h"
#include "../Input/Input.h"
#include "../Logging/Logger.h"
#include "../Core/Macros.h"

namespace adria
{
	CameraManager::CameraManager(Input& input) : input{ input }
	{
	}

	void CameraManager::AddCamera(CameraParameters const& camera_desc)
	{
		cameras.emplace_back(camera_desc);
	}
	void CameraManager::Update(float32 dt)
	{
		
		if (cameras.empty() || !update_camera) return;

		auto& camera = cameras[current_camera];

		if (input.GetKey(EKeyCode::Space)) return;

		float speed_factor = 1.0f;

		if (input.GetKey(EKeyCode::ShiftLeft)) speed_factor *= 5.0f;
		if (input.GetKey(EKeyCode::CtrlLeft))  speed_factor *= 0.2f;

		if (input.GetKey(EKeyCode::W))
			camera.Walk(speed_factor * dt);

		if (input.GetKey(EKeyCode::S))
			camera.Walk(-speed_factor * dt);

		if (input.GetKey(EKeyCode::A))
			camera.Strafe(-speed_factor * dt);

		if (input.GetKey(EKeyCode::D))
			camera.Strafe(speed_factor * dt);

		if (input.GetKey(EKeyCode::Q))
			camera.Jump(speed_factor * dt);

		if (input.GetKey(EKeyCode::E))
			camera.Jump(-speed_factor * dt);

		if (input.GetKey(EKeyCode::MouseRight))
		{
			float32 dx = input.GetMouseDeltaX();
			float32 dy = input.GetMouseDeltaY();
			camera.Pitch((int64)dy);
			camera.Yaw((int64)dx);
		}

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
	void CameraManager::OnResize(uint32 width, uint32 height)
	{
		for (auto& camera : cameras)
			camera.OnResize(width, height);

	}
	void CameraManager::OnScroll(int32 scroll)
	{
		if (update_camera)
		{
			auto& camera = cameras[current_camera];
			camera.Zoom((float32)scroll);
		}
	}
	void CameraManager::ShouldUpdate(bool update)
	{
		update_camera = update;
	}
}