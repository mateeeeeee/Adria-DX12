#include <algorithm>
#include "Camera.h"
#include "Core/Input.h"
#include "Math/Constants.h"
#include "Math/Halton.h"

using namespace DirectX;

namespace adria
{

	Camera::Camera(CameraParameters const& desc) : position{ desc.position }, right_vector{ 1.0f,0.0f,0.0f }, up_vector{ 0.0f,1.0f,0.0f },
		aspect_ratio{ desc.aspect_ratio }, fov{ desc.fov }, near_plane{ desc.far_plane }, far_plane{ desc.near_plane },
		speed{ desc.speed }, sensitivity{ desc.sensitivity }
	{
		look_vector = desc.look_at - position;
		UpdateViewMatrix();
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}

	Vector2 Camera::Jitter(uint32 frame_index) const
	{
		Vector2 jitter{};
		constexpr HaltonSequence<16, 2> x;
		constexpr HaltonSequence<16, 3> y;
		jitter.x = x[frame_index % 16] - 0.5f;
		jitter.y = y[frame_index % 16] - 0.5f;
		return jitter;
	}
	float Camera::Near() const
	{
		return near_plane;
	}
	float Camera::Far() const
	{
		return far_plane;
	}
	float Camera::Fov() const
	{
		return fov;
	}
	float Camera::AspectRatio() const
	{
		return aspect_ratio;
	}

	void Camera::Tick(float dt)
	{
		if (!enabled) return;
		Input& input = g_Input;
		if (input.GetKey(KeyCode::Space)) return;

		float speed_factor = 1.0f;

		if (input.GetKey(KeyCode::ShiftLeft)) speed_factor *= 5.0f;
		if (input.GetKey(KeyCode::CtrlLeft))  speed_factor *= 0.2f;

		if (input.GetKey(KeyCode::W)) Walk(speed_factor * dt);
		if (input.GetKey(KeyCode::S)) Walk(-speed_factor * dt);
		if (input.GetKey(KeyCode::A)) Strafe(-speed_factor * dt);
		if (input.GetKey(KeyCode::D)) Strafe(speed_factor * dt);
		if (input.GetKey(KeyCode::Q)) Jump(speed_factor * dt);
		if (input.GetKey(KeyCode::E)) Jump(-speed_factor * dt);
		if (input.GetKey(KeyCode::MouseRight))
		{
			float dx = input.GetMouseDeltaX();
			float dy = input.GetMouseDeltaY();
			Pitch((int64)dy);
			Yaw((int64)dx);
		}
		UpdateViewMatrix();
	}
	void Camera::Zoom(int32 increment)
	{
		if (!enabled) return;
		fov -= XMConvertToRadians(increment * 1.0f);
		fov = std::clamp(fov, 0.00005f, pi_div_2<float>);
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}
	void Camera::OnResize(uint32 w, uint32 h)
	{
		SetAspectRatio(static_cast<float>(w) / h);
	}

	void Camera::SetAspectRatio(float ar)
	{
		aspect_ratio = ar;
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}
	void Camera::SetFov(float _fov)
	{
		fov = _fov;
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}
	void Camera::SetNearAndFar(float n, float f)
	{
		near_plane = n;
		far_plane = f;
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}
	void Camera::SetPosition(Vector3 const& pos)
	{
		position = pos;
	}

	Matrix Camera::View() const
	{
		return view_matrix;
	}
	Matrix Camera::Proj() const
	{
		return projection_matrix;
	}
	Matrix Camera::ViewProj() const
	{
		return view_matrix * projection_matrix;
	}
	BoundingFrustum Camera::Frustum() const
	{
		BoundingFrustum frustum(Proj());
		if (frustum.Far < frustum.Near) std::swap(frustum.Far, frustum.Near);
		frustum.Transform(frustum, view_matrix.Invert());
		return frustum;
	}

	void Camera::UpdateViewMatrix()
	{
		look_vector.Normalize();
		right_vector = up_vector.Cross(look_vector);
		right_vector.Normalize();
		up_vector = look_vector.Cross(right_vector);
		up_vector.Normalize();
		SetView();
	}

	void Camera::Strafe(float dt)
	{
		position += dt * speed * right_vector;
	}
	void Camera::Walk(float dt)
	{
		position += dt * speed * look_vector;
	}
	void Camera::Jump(float dt)
	{
		position += dt * speed * up_vector;
	}
	void Camera::Pitch(int64 dy)
	{
		Matrix R = Matrix::CreateFromAxisAngle(right_vector, sensitivity * XMConvertToRadians((float)dy));
		up_vector = Vector3::TransformNormal(up_vector, R);
		look_vector = Vector3::TransformNormal(look_vector, R);
	}
	void Camera::Yaw(int64 dx)
	{
		Matrix R = Matrix::CreateRotationY(sensitivity * XMConvertToRadians((float)dx));

		right_vector = Vector3::TransformNormal(right_vector, R);
		up_vector = Vector3::TransformNormal(up_vector, R);
		look_vector = Vector3::TransformNormal(look_vector, R);
	}
	void Camera::SetLens(float fov, float aspect, float zn, float zf)
	{
		projection_matrix = XMMatrixPerspectiveFovLH(fov, aspect, zn, zf);
	}
	void Camera::SetView()
	{
		view_matrix = XMMatrixLookToLH(position, look_vector, up_vector);
	}
}