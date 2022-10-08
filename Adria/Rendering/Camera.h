#pragma once
#include <DirectXCollision.h>
#include <utility>
#include "../Core/Definitions.h"


namespace adria
{

	struct CameraParameters
	{
		float32 aspect_ratio;
		float32 near_plane;
		float32 far_plane;
		float32 fov;
		float32 speed;
		float32 sensitivity;
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

		float32 Near() const;
		float32 Far() const;
		float32 Fov() const;
		float32 AspectRatio() const;

		void SetPosition(DirectX::XMFLOAT3 pos);
		void SetNearAndFar(float32 n, float32 f);
		void SetAspectRatio(float32 ar);
		void SetFov(float32 fov);

		void Zoom(int32 increment);
		void OnResize(uint32 w, uint32 h);
		void Tick(float32 dt);
		void Enable(bool _enabled) { enabled = _enabled; }
	private:

		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 right_vector;
		DirectX::XMFLOAT3 up_vector;
		DirectX::XMFLOAT3 look_vector;
		DirectX::XMFLOAT4X4 view_matrix;
		DirectX::XMFLOAT4X4 projection_matrix;

		float32 aspect_ratio;
		float32 fov;
		float32 near_plane, far_plane;  
		float32 speed;
		float32 sensitivity;
		bool	enabled;
	private:

		void UpdateViewMatrix();
		void Strafe(float32 dt);
		void Walk(float32 dt);
		void Jump(float32 dt);
		void Pitch(int64 dy);
		void Yaw(int64 dx);
		void SetLens(float32 fov, float32 aspect, float32 zn, float32 zf);
		void SetView();
	};

}