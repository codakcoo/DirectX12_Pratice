#include "Camera.h"

Camera::Camera()
{
}

DirectX::XMVECTOR Camera::GetPosition() const
{
	return XMLoadFloat3(&mPosition);
}

void Camera::SetPosition(float x, float y, float z)
{
	mPosition = { x, y, z };
	mViewDirty = true;
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

// 투영 행렬
void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	XMMATRIX P = XMMatrixPerspectiveFovLH(fovY, aspect, zn, zf);
	XMStoreFloat4x4(&mProj, P);
}

void Camera::Walk(float d)
{
	// mPosition += d * mLook (앞 방향으로 d만큼)
	XMVECTOR s = XMVectorReplicate(d);					// (d,d,d,d)
	XMVECTOR l = XMLoadFloat3(&mLook);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));			// p + d*l
	mViewDirty = true;
}

void Camera::Strafe(float d)
{
	// mPosition += d * mRight (오른쪽 방향으로 d만큼)
	XMVECTOR s = XMVectorReplicate(d);					// (d,d,d,d)
	XMVECTOR r = XMLoadFloat3(&mRight);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));			// p + d*r
	mViewDirty = true;
}

void Camera::Pitch(float angle)
{
	// 누적 pitch를 -89도 ~ + 89도로 제한
	float newPitch = mPitchAngle + angle;
	float limit = XMConvertToRadians(89.0f);

	if (newPitch > limit)
		angle = limit - mPitchAngle;			// limit까지만 회전
	else if (newPitch < -limit)
		angle = -limit - mPitchAngle;

	mPitchAngle += angle;

	// mRight 축을 기준으로 Up, Look을 회전 (고개 끄덕임)
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

	XMStoreFloat3(&mUp,		XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook,	XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
	mViewDirty = true;
}

void Camera::RotateY(float angle)
{
	// 월드 Y축 기준으로 세 축 모두 회전 (고개 좌우로 돌림)
	XMMATRIX R = XMMatrixRotationY(angle);

	XMStoreFloat3(&mRight,	XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp,		XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook,	XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
	mViewDirty = true;
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
	return XMLoadFloat4x4(&mView);
}

DirectX::XMMATRIX Camera::GetProj() const
{
	return XMLoadFloat4x4(&mProj);
}
