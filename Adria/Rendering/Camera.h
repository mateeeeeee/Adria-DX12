#pragma once
#include <DirectXCollision.h>
#include <utility>
#include "../Core/Definitions.h"


namespace adria
{

	struct camera_desc_t
	{
		float32 aspect_ratio;
		float32 near_plane;
		float32 far_plane;
		float32 fov;
		float32 position_x;
		float32 position_y;
		float32 position_z;
	};

	class Camera
	{
		friend class CameraManager;

		void SetLens(float32 fov, float32 aspect, float32 zn, float32 zf);

		void SetView();

		float32 SPEED = 125.0f;

		float32 SENSITIVITY = 0.3f;

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

		float32 Near() const;

		float32 Far() const;

		float32 Fov() const;

		float32 AspectRatio() const;

		void SetPosition(DirectX::XMFLOAT3 pos);

		void SetNearAndFar(float32 n, float32 f);

		void SetAspectRatio(float32 ar);

		void SetFov(float32 fov);

	private:

		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 right_vector;
		DirectX::XMFLOAT3 up_vector;
		DirectX::XMFLOAT3 look_vector;
		DirectX::XMFLOAT4X4 view_matrix;
		DirectX::XMFLOAT4X4 projection_matrix;

		float32 aspect_ratio;
		float32 fov;
		float32 _near, _far;  

		float32 speed = 25.0f;
		float32 sensitivity = 0.3f;


	private:

		void UpdateViewMatrix();

		void Strafe(float32 dt);

		void Walk(float32 dt);

		void Jump(float32 dt);

		void Pitch(int64 dy);

		void Yaw(int64 dx);

		void Zoom(float32 increment);

		void OnResize(uint32 w, uint32 h);
	};

}