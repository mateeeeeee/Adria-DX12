#include "Camera.h"
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

	XMVECTOR Camera::Position() const
	{
		return XMLoadFloat3(&position);
	}

	float32 Camera::Near() const
	{
		return near_plane;
	}

	float32 Camera::Far() const
	{
		return far_plane;
	}


	void Camera::SetLens(float32 fov, float32 aspect, float32 zn, float32 zf)
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

	float32 Camera::Fov() const
	{
		return fov;
	}

	float32 Camera::AspectRatio() const
	{
		return aspect_ratio;
	}

	void Camera::SetPosition(XMFLOAT3 pos)
	{
		position = pos;
	}

	void Camera::SetNearAndFar(float32 near, float32 far)
	{
		near_plane = near;
		far_plane = far;
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}

	void Camera::UpdateViewMatrix()
	{
		XMVECTOR R = XMLoadFloat3(&right_vector);
		XMVECTOR U = XMLoadFloat3(&up_vector);
		XMVECTOR L = XMLoadFloat3(&look_vector);
		XMVECTOR P = XMLoadFloat3(&position);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product.
		R = XMVector3Cross(U, L);

		// Fill in the view matrix entries.
		XMStoreFloat3(&right_vector, R);
		XMStoreFloat3(&up_vector, U);
		XMStoreFloat3(&look_vector, L);

		SetView();
	}

	void Camera::Strafe(float32 dt)
	{

		// mPosition += d*mRight
		XMVECTOR s = XMVectorReplicate(dt * speed);
		XMVECTOR r = XMLoadFloat3(&right_vector);
		XMVECTOR p = XMLoadFloat3(&position);
		XMStoreFloat3(&position, XMVectorMultiplyAdd(s, r, p));
	}

	void Camera::Walk(float32 dt)
	{
		// mPosition += d*mLook
		XMVECTOR s = XMVectorReplicate(dt * speed);
		XMVECTOR l = XMLoadFloat3(&look_vector);
		XMVECTOR p = XMLoadFloat3(&position);
		XMStoreFloat3(&position, XMVectorMultiplyAdd(s, l, p));

	}

	void Camera::Jump(float32 dt)
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
		XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&right_vector), sensitivity * XMConvertToRadians((float32)dy));
		XMStoreFloat3(&up_vector, XMVector3TransformNormal(XMLoadFloat3(&up_vector), R));
		XMStoreFloat3(&look_vector, XMVector3TransformNormal(XMLoadFloat3(&look_vector), R));
	}

	void Camera::Yaw(int64 dx)
	{
		// Rotate the basis vectors about the world y-axis.

		XMMATRIX R = XMMatrixRotationY(sensitivity * XMConvertToRadians((float32)dx));

		XMStoreFloat3(&right_vector, XMVector3TransformNormal(XMLoadFloat3(&right_vector), R));
		XMStoreFloat3(&up_vector, XMVector3TransformNormal(XMLoadFloat3(&up_vector), R));
		XMStoreFloat3(&look_vector, XMVector3TransformNormal(XMLoadFloat3(&look_vector), R));
	}

	void Camera::Zoom(float32 increment)
	{
		fov -= XMConvertToRadians(increment);
		fov = std::clamp(fov, 0.00005f, pi_div_2<float32>);
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}

	void Camera::SetAspectRatio(float32 ar)
	{
		aspect_ratio = ar;
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}

	void Camera::SetFov(float32 _fov)
	{
		fov = _fov;
		SetLens(fov, aspect_ratio, near_plane, far_plane);
	}

	
	void Camera::OnResize(uint32 w, uint32 h)
	{
		SetAspectRatio(static_cast<float32>(w) / h);
	}

	XMMATRIX Camera::View() const
	{
		return XMLoadFloat4x4(&view_matrix);
	}

	XMMATRIX Camera::Proj() const
	{
		return XMLoadFloat4x4(&projection_matrix);
	}


}