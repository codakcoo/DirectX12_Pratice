#include "Camera.h"

Camera::Camera()
{
}

DirectX::XMVECTOR Camera::GetPosition() const
{
	return DirectX::XMVECTOR();
}

void Camera::SetPosition(float x, float y, float z)
{
}

DirectX::XMVECTOR Camera::GetRight() const
{
	return XMLoadFloat3(&mRight);
}

DirectX::XMVECTOR Camera::GetUp() const
{
	return XMLoadFloat3(&mUp);
}

DirectX::XMVECTOR Camera::GetLook() const
{
	return XMLoadFloat3(&mLook);
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
}

void Camera::Walk(float d)
{
}

void Camera::Strafe(float d)
{
}

void Camera::Picth(float angle)
{
}

void Camera::RotateY(float angle)
{
}

void Camera::UpdateViewMatrix()
{
	if (!mViewDirty) return;

	XMVECTOR R = XMLoadFloat3(&mRight);
	XMVECTOR U = XMLoadFloat3(&mUp);
	XMVECTOR L = XMLoadFloat3(&mLook);
	XMVECTOR P = XMLoadFloat3(&mPosition);

	// 세 축을 직교 정규화 (회전 누적 오차 보정)
	L = XMVector3Normalize(L);
	U = XMVector3Normalize(XMVector3Cross(L, R));				// Up = Look * Right
	R = XMVector3Cross(U, L);									// Right = Up * Look

	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);
	XMStoreFloat3(&mLook, L);

	// 뷰 행렬 구성 (직접)
	float x = -XMVectorGetX(XMVector3Dot(P, R));				// Pos dot Right
	float y = -XMVectorGetX(XMVector3Dot(P, U));				// Pos dot Up
	float z = -XMVectorGetX(XMVector3Dot(P, L));				// Pos dot Look

	mView(0, 0) = mRight.x;		mView(1, 0) = mRight.y;		mView(2, 0) = mRight.z;		mView(3, 0) = x;
	mView(0, 1) = mUp.x;		mView(1, 1) = mUp.y;		mView(2, 1) = mUp.z;		mView(3, 1) = y;
	mView(0, 2) = mLook.x;		mView(1, 2) = mLook.y;		mView(2, 2) = mLook.z;		mView(3, 2) = z;
	mView(0, 3) = 0.0f;			mView(1, 3) = 0.0f;			mView(2, 3) = 0.0f;			mView(3, 3) = 1.0f;

	mViewDirty = false;
}

DirectX::XMMATRIX Camera::GetView() const
{
	return DirectX::XMMATRIX();
}

DirectX::XMMATRIX Camera::GetProj() const
{
	return DirectX::XMMATRIX();
}
