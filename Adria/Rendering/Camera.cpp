#include "Camera.h"
#include "../Input/Input.h"
#include "../Math/Constants.h"
#include <algorithm>

using namespace DirectX;

namespace adria
{

	Camera::Camera(CameraParameters const& desc) : position{ desc.position }, right_vector{ 1.0f,0.0f,0.0f }, up_vector{ 0.0f,1.0f,0.0f },
		look_vector{ desc.look_at }, aspect_ratio{ desc.aspect_ratio }, fov{ desc.fov }, near_plane{ desc.near_plane }, far_plane{ desc.far_plane },
		speed{ desc.speed }, sensitivity{ desc.sensitivity }
	{
		SetView();
		SetLens(fov, aspect_ratio, near_plane, far_plane);
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
		Input& input = Input::GetInstance();
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
	void Camera::SetPosition(XMFLOAT3 pos)
	{
		position = pos;
	}

	XMMATRIX Camera::View() const
	{
		return XMLoadFloat4x4(&view_matrix);
	}
	XMMATRIX Camera::Proj() const
	{
		return XMLoadFloat4x4(&projection_matrix);
	}
	XMMATRIX Camera::ViewProj() const
	{
		return XMMatrixMultiply(View(), Proj());
	}
	BoundingFrustum Camera::Frustum() const
	{
		BoundingFrustum frustum(Proj());
		frustum.Transform(frustum, XMMatrixInverse(nullptr, View()));
		return frustum;
	}

	void Camera::UpdateViewMatrix()
	{
		XMVECTOR R = XMLoadFloat3(&right_vector);
		XMVECTOR U = XMLoadFloat3(&up_vector);
		XMVECTOR L = XMLoadFloat3(&look_vector);
		XMVECTOR P = XMLoadFloat3(&position);

		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		R = XMVector3Cross(U, L);

		XMStoreFloat3(&right_vector, R);
		XMStoreFloat3(&up_vector, U);
		XMStoreFloat3(&look_vector, L);
		SetView();
	}

	void Camera::Strafe(float dt)
	{
		// mPosition += d*mRight
		XMVECTOR s = XMVectorReplicate(dt * speed);
		XMVECTOR r = XMLoadFloat3(&right_vector);
		XMVECTOR p = XMLoadFloat3(&position);
		XMStoreFloat3(&position, XMVectorMultiplyAdd(s, r, p));
	}
	void Camera::Walk(float dt)
	{
		// mPosition += d*mLook
		XMVECTOR s = XMVectorReplicate(dt * speed);
		XMVECTOR l = XMLoadFloat3(&look_vector);
		XMVECTOR p = XMLoadFloat3(&position);
		XMStoreFloat3(&position, XMVectorMultiplyAdd(s, l, p));
	}
	void Camera::Jump(float dt)
	{
		// mPosition += d*Up
		XMVECTOR s = XMVectorReplicate(dt * speed);
		XMVECTOR l = XMLoadFloat3(&up_vector);
		XMVECTOR p = XMLoadFloat3(&position);
		XMStoreFloat3(&position, XMVectorMultiplyAdd(s, l, p));
	}
	void Camera::Pitch(int64 dy)
	{
		// Rotate up and look vector about the right vector.
		XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&right_vector), sensitivity * XMConvertToRadians((float)dy));
		XMStoreFloat3(&up_vector, XMVector3TransformNormal(XMLoadFloat3(&up_vector), R));
		XMStoreFloat3(&look_vector, XMVector3TransformNormal(XMLoadFloat3(&look_vector), R));
	}
	void Camera::Yaw(int64 dx)
	{
		// Rotate the basis vectors about the world y-axis.

		XMMATRIX R = XMMatrixRotationY(sensitivity * XMConvertToRadians((float)dx));

		XMStoreFloat3(&right_vector, XMVector3TransformNormal(XMLoadFloat3(&right_vector), R));
		XMStoreFloat3(&up_vector, XMVector3TransformNormal(XMLoadFloat3(&up_vector), R));
		XMStoreFloat3(&look_vector, XMVector3TransformNormal(XMLoadFloat3(&look_vector), R));
	}
	void Camera::SetLens(float fov, float aspect, float zn, float zf)
	{
		XMMATRIX P = XMMatrixPerspectiveFovLH(fov, aspect, zn, zf);
		XMStoreFloat4x4(&projection_matrix, P);
	}
	void Camera::SetView()
	{
		XMVECTOR pos = XMLoadFloat3(&position);
		XMVECTOR look = XMLoadFloat3(&look_vector);
		XMVECTOR up = XMLoadFloat3(&up_vector);
		XMMATRIX view = XMMatrixLookToLH(pos, look, up);
		XMStoreFloat4x4(&view_matrix, view);
	}
}