#pragma once
#include <DirectXCollision.h>
#include <utility>
#include "Core/CoreTypes.h"

namespace adria
{

	struct CameraParameters
	{
		float aspect_ratio;
		float near_plane;
		float far_plane;
		float fov;
		float speed;
		float sensitivity;
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 look_at;
	};

	class Camera
	{
	public:
		Camera() = default;
		explicit Camera(CameraParameters const&);

		DirectX::XMMATRIX View() const;
		DirectX::XMMATRIX Proj() const;
		DirectX::XMMATRIX ViewProj() const;
		DirectX::BoundingFrustum Frustum() const;

		DirectX::XMVECTOR Position() const
		{
			return XMLoadFloat3(&position);
		}
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

		float Near() const;
		float Far() const;
		float Fov() const;
		float AspectRatio() const;

		void SetPosition(DirectX::XMFLOAT3 pos);
		void SetNearAndFar(float n, float f);
		void SetAspectRatio(float ar);
		void SetFov(float fov);

		void Zoom(int32 increment);
		void OnResize(uint32 w, uint32 h);
		void Tick(float dt);
		void Enable(bool _enabled) { enabled = _enabled; }
	private:

		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 right_vector;
		DirectX::XMFLOAT3 up_vector;
		DirectX::XMFLOAT3 look_vector;
		DirectX::XMFLOAT4X4 view_matrix;
		DirectX::XMFLOAT4X4 projection_matrix;

		float aspect_ratio;
		float fov;
		float near_plane, far_plane;  
		float speed;
		float sensitivity;
		bool  enabled;
	private:

		void UpdateViewMatrix();
		void Strafe(float dt);
		void Walk(float dt);
		void Jump(float dt);
		void Pitch(int64 dy);
		void Yaw(int64 dx);
		void SetLens(float fov, float aspect, float zn, float zf);
		void SetView();
	};

}