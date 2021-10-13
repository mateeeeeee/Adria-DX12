#include "Camera.h"
#include "../Math/Constants.h"
#include <algorithm>


namespace adria
{

	constexpr f32 DEGREES_TO_RADIANS(f32 degrees)
	{
		return (degrees * pi<f32> / 180.0f);
	}

	Camera::Camera(camera_desc_t const& desc) : position{ desc.position_x, desc.position_y, desc.position_z }, right_vector{ 1.0f,0.0f,0.0f }, up_vector{ 0.0f,1.0f,0.0f },
		look_vector{ 0.0f,0.0f,1.0f }, aspect_ratio{ desc.aspect_ratio }, fov{ desc.fov }, _near{ desc.near_plane }, _far{ desc.far_plane }
	{
		SetView();
		SetLens(fov, aspect_ratio, _near, _far);
	}

	DirectX::XMVECTOR Camera::Position() const
	{
		return DirectX::XMLoadFloat3(&position);
	}

	f32 Camera::Near() const
	{
		return _near;
	}

	f32 Camera::Far() const
	{
		return _far;
	}


	void Camera::SetLens(f32 fov, f32 aspect, f32 zn, f32 zf)
	{
		DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, zn, zf);
		XMStoreFloat4x4(&projection_matrix, P);
	}

	void Camera::SetView()
	{
		DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&position);
		DirectX::XMVECTOR look = DirectX::XMLoadFloat3(&look_vector);
		DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&up_vector);
		DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(pos, look, up);
		DirectX::XMStoreFloat4x4(&view_matrix, view);
	}

	DirectX::XMMATRIX Camera::ViewProj() const
	{
		return DirectX::XMMatrixMultiply(View(), Proj());
	}

	DirectX::BoundingFrustum Camera::Frustum() const
	{
		DirectX::BoundingFrustum frustum(Proj());
		frustum.Transform(frustum, DirectX::XMMatrixInverse(nullptr, View()));
		return frustum;
	}

	f32 Camera::Fov() const
	{
		return fov;
	}

	f32 Camera::AspectRatio() const
	{
		return aspect_ratio;
	}

	void Camera::SetPosition(DirectX::XMFLOAT3 pos)
	{
		position = pos;
	}

	void Camera::SetNearAndFar(f32 near, f32 far)
	{
		_near = near;
		_far = far;
		SetLens(fov, aspect_ratio, _near, _far);
	}

	void Camera::UpdateViewMatrix()
	{
		DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&right_vector);
		DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&up_vector);
		DirectX::XMVECTOR L = DirectX::XMLoadFloat3(&look_vector);
		DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&position);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = DirectX::XMVector3Normalize(L);
		U = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product.
		R = DirectX::XMVector3Cross(U, L);

		// Fill in the view matrix entries.
		DirectX::XMStoreFloat3(&right_vector, R);
		DirectX::XMStoreFloat3(&up_vector, U);
		DirectX::XMStoreFloat3(&look_vector, L);

		SetView();
	}

	void Camera::Strafe(f32 dt)
	{

		// mPosition += d*mRight
		DirectX::XMVECTOR s = DirectX::XMVectorReplicate(dt * SPEED);
		DirectX::XMVECTOR r = DirectX::XMLoadFloat3(&right_vector);
		DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&position);
		XMStoreFloat3(&position, DirectX::XMVectorMultiplyAdd(s, r, p));
	}

	void Camera::Walk(f32 dt)
	{
		// mPosition += d*mLook
		DirectX::XMVECTOR s = DirectX::XMVectorReplicate(dt * SPEED);
		DirectX::XMVECTOR l = DirectX::XMLoadFloat3(&look_vector);
		DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&position);
		DirectX::XMStoreFloat3(&position, DirectX::XMVectorMultiplyAdd(s, l, p));

	}

	void Camera::Jump(f32 dt)
	{
		// mPosition += d*Up
		DirectX::XMVECTOR s = DirectX::XMVectorReplicate(dt * SPEED);
		DirectX::XMVECTOR l = DirectX::XMLoadFloat3(&up_vector);
		DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&position);
		DirectX::XMStoreFloat3(&position, DirectX::XMVectorMultiplyAdd(s, l, p));
	}

	void Camera::Pitch(i64 dy)
	{
		// Rotate up and look vector about the right vector.
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(XMLoadFloat3(&right_vector), SENSITIVITY * DEGREES_TO_RADIANS((f32)dy));
		DirectX::XMStoreFloat3(&up_vector, XMVector3TransformNormal(XMLoadFloat3(&up_vector), R));
		DirectX::XMStoreFloat3(&look_vector, XMVector3TransformNormal(XMLoadFloat3(&look_vector), R));
	}

	void Camera::Yaw(i64 dx)
	{
		// Rotate the basis vectors about the world y-axis.

		DirectX::XMMATRIX R = DirectX::XMMatrixRotationY(SENSITIVITY * DEGREES_TO_RADIANS((f32)dx));

		DirectX::XMStoreFloat3(&right_vector, XMVector3TransformNormal(XMLoadFloat3(&right_vector), R));
		DirectX::XMStoreFloat3(&up_vector, XMVector3TransformNormal(XMLoadFloat3(&up_vector), R));
		DirectX::XMStoreFloat3(&look_vector, XMVector3TransformNormal(XMLoadFloat3(&look_vector), R));
	}

	void Camera::Zoom(f32 increment)
	{
		fov -= DEGREES_TO_RADIANS(increment);
		fov = std::clamp(fov, 0.00005f, pi_div_2<f32>);
		SetLens(fov, aspect_ratio, _near, _far);
	}

	void Camera::SetAspectRatio(f32 ar)
	{
		aspect_ratio = ar;
		SetLens(fov, aspect_ratio, _near, _far);
	}

	void Camera::SetFov(f32 _fov)
	{
		fov = _fov;
		SetLens(fov, aspect_ratio, _near, _far);
	}

	
	void Camera::OnResize(u32 w, u32 h)
	{
		SetAspectRatio(static_cast<f32>(w) / h);
	}

	DirectX::XMMATRIX Camera::View() const
	{
		return DirectX::XMLoadFloat4x4(&view_matrix);
	}

	DirectX::XMMATRIX Camera::Proj() const
	{
		return DirectX::XMLoadFloat4x4(&projection_matrix);
	}


}