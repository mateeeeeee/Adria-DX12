#pragma once
#include <DirectXCollision.h>
#include <utility>
#include "../Core/Definitions.h"


namespace adria
{

	struct camera_desc_t
	{
		f32 aspect_ratio;
		f32 near_plane;
		f32 far_plane;
		f32 fov;
		f32 position_x;
		f32 position_y;
		f32 position_z;
	};

	class Camera
	{
		friend class CameraManager;

		void SetLens(f32 fov, f32 aspect, f32 zn, f32 zf);

		void SetView();

		f32 SPEED = 125.0f;

		f32 SENSITIVITY = 0.3f;

	public:

		Camera() = default;

		explicit Camera(camera_desc_t const&);

		DirectX::XMVECTOR Position() const;

		DirectX::XMMATRIX View() const;

		DirectX::XMMATRIX Proj() const;

		DirectX::XMMATRIX ViewProj() const;

		DirectX::BoundingFrustum Frustum() const;

		DirectX::XMVECTOR Up() const
		{
			return DirectX::XMLoadFloat3(&up_vector);
		}

		DirectX::XMVECTOR Right() const
		{
			return DirectX::XMLoadFloat3(&right_vector);
		}

		DirectX::XMVECTOR Forward() const
		{
			return DirectX::XMLoadFloat3(&look_vector);
		}

		f32 Near() const;

		f32 Far() const;

		f32 Fov() const;

		f32 AspectRatio() const;

		void SetPosition(DirectX::XMFLOAT3 pos);

		void SetNearAndFar(f32 n, f32 f);

		void SetAspectRatio(f32 ar);

		void SetFov(f32 fov);

	private:

		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 right_vector;
		DirectX::XMFLOAT3 up_vector;
		DirectX::XMFLOAT3 look_vector;
		DirectX::XMFLOAT4X4 view_matrix;
		DirectX::XMFLOAT4X4 projection_matrix;

		f32 aspect_ratio;
		f32 fov;
		f32 _near, _far;  

		f32 speed = 25.0f;
		f32 sensitivity = 0.3f;


	private:

		void UpdateViewMatrix();

		void Strafe(f32 dt);

		void Walk(f32 dt);

		void Jump(f32 dt);

		void Pitch(i64 dy);

		void Yaw(i64 dx);

		void Zoom(f32 increment);

		void OnResize(u32 w, u32 h);
	};

}