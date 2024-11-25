#pragma once
#include <utility>

namespace adria
{

	struct CameraParameters
	{
		Float near_plane;
		Float far_plane;
		Float fov;
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

		Vector2 Jitter(Uint32 frame_index) const;
		Float Near() const;
		Float Far() const;
		Float Fov() const;
		Float AspectRatio() const;

		void SetPosition(Vector3 const& pos);
		void SetNearAndFar(Float n, Float f);
		void SetAspectRatio(Float ar);
		void SetFov(Float fov);

		void Zoom(Int32 increment);
		void OnResize(Uint32 w, Uint32 h);
		void Update(Float dt);
		void Enable(Bool _enabled) { enabled = _enabled; }
		Bool IsChanged() const { return changed; }
	private:
		Matrix view_matrix;
		Matrix projection_matrix;

		Vector3 position;
		Vector3 velocity;
		Quaternion orientation;
		Vector3 look_vector;

		Float fov;
		Float aspect_ratio;
		Float near_plane, far_plane;
		Bool  enabled;
		Bool  changed;

	private:
		void SetProjectionMatrix(Float fov, Float aspect, Float zn, Float zf);
	};

}