#pragma once
#include <DirectXMath.h>

using namespace DirectX;

class Camera
{
public:
	Camera();

	// 카메라 위치
	DirectX::XMVECTOR GetPosition() const;
	void SetPosition(float x, float y, float z);

	// 카메라 좌표축 (오른쪽/위/앞)
	DirectX::XMVECTOR GetRight() const;
	DirectX::XMVECTOR GetUp() const;
	DirectX::XMVECTOR GetLook() const;

	// 투영 설정
	void SetLens(float fovY, float aspect, float zn, float zf);

	// 이동 (거리 d만큼)
	void Walk(float d);				// 앞뒤
	void Strafe(float d);			// 좌우

	// 회전 (각도)
	void Picth(float angle);		// 상하
	void RotateY(float angle);		// 좌우

	// 뷰 행렬 갱신 (매 프레임 이동/회전 후 호출)
	void UpdateViewMatrix();

	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProj() const;

private:
	DirectX::XMFLOAT3 mPosition		= { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mRight		= { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mUp			= { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mLook			= { 0.0f, 0.0f, 1.0f };

	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;

	bool mViewDirty = true;
};

