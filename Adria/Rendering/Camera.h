#pragma once
#include <utility>

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
		Vector3 position;
		Vector3 look_at;
	};

	class Camera
	{
	public:
		Camera() = default;
		explicit Camera(CameraParameters const&);

		Matrix View() const;
		Matrix Proj() const;
		Matrix ViewProj() const;
		BoundingFrustum Frustum() const;

		Vector3 Position() const
		{
			return position;
		}
		Vector3 Up() const
		{
			return up_vector;
		}
		Vector3 Right() const
		{
			return right_vector;
		}
		Vector3 Forward() const
		{
			return look_vector;
		}

		Vector2 Jitter(uint32 frame_index) const;
		float Near() const;
		float Far() const;
		float Fov() const;
		float AspectRatio() const;

		void SetPosition(Vector3 const& pos);
		void SetNearAndFar(float n, float f);
		void SetAspectRatio(float ar);
		void SetFov(float fov);

		void Zoom(int32 increment);
		void OnResize(uint32 w, uint32 h);
		void Tick(float dt);
		void Enable(bool _enabled) { enabled = _enabled; }
	private:

		Vector3 position;
		Vector3 right_vector;
		Vector3 up_vector;
		Vector3 look_vector;
		Matrix view_matrix;
		Matrix projection_matrix;

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