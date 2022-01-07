#pragma once
#include <DirectXCollision.h>
#include <utility>
#include "../Core/Definitions.h"


namespace adria
{

	struct camera_desc_t
	{
		F32 aspect_ratio;
		F32 near_plane;
		F32 far_plane;
		F32 fov;
		F32 position_x;
		F32 position_y;
		F32 position_z;
	};

	class Camera
	{
		friend class CameraManager;

		void SetLens(F32 fov, F32 aspect, F32 zn, F32 zf);

		void SetView();

		F32 SPEED = 125.0f;

		F32 SENSITIVITY = 0.3f;

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

		F32 Near() const;

		F32 Far() const;

		F32 Fov() const;

		F32 AspectRatio() const;

		void SetPosition(DirectX::XMFLOAT3 pos);

		void SetNearAndFar(F32 n, F32 f);

		void SetAspectRatio(F32 ar);

		void SetFov(F32 fov);

	private:

		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 right_vector;
		DirectX::XMFLOAT3 up_vector;
		DirectX::XMFLOAT3 look_vector;
		DirectX::XMFLOAT4X4 view_matrix;
		DirectX::XMFLOAT4X4 projection_matrix;

		F32 aspect_ratio;
		F32 fov;
		F32 _near, _far;  

		F32 speed = 25.0f;
		F32 sensitivity = 0.3f;


	private:

		void UpdateViewMatrix();

		void Strafe(F32 dt);

		void Walk(F32 dt);

		void Jump(F32 dt);

		void Pitch(I64 dy);

		void Yaw(I64 dx);

		void Zoom(F32 increment);

		void OnResize(U32 w, U32 h);
	};

}