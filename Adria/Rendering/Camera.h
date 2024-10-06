#pragma once
#include <utility>

namespace adria
{

	struct CameraParameters
	{
		float near_plane;
		float far_plane;
		float fov;
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
		Vector3 Forward() const;

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
		void Update(float dt);
		void Enable(bool _enabled) { enabled = _enabled; }
		bool IsChanged() const { return changed; }
	private:
		Matrix view_matrix;
		Matrix projection_matrix;

		Vector3 position;
		Vector3 velocity;
		Quaternion orientation;
		Vector3 look_vector;

		float fov;
		float aspect_ratio;
		float near_plane, far_plane;
		bool  enabled;
		bool  changed;

	private:
		void SetProjectionMatrix(float fov, float aspect, float zn, float zf);
	};

}