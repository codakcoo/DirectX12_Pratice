#pragma once
#include <DirectXMath.h>
#include <vector>

using namespace std;
using namespace DirectX;

struct Keyframe
{
	float TimePos			= 0.0f;
	XMFLOAT3 Translation	= { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 Scale			= { 1.0f, 1.0f, 1.0f };
	XMFLOAT4 RotationQuat	= { 0.0f, 0.0f, 0.0f, 1.0f };			// 항등 쿼터니언
};

// ch23에서 본(bone) 하나의 애니메이션이 이 구조 그대로
struct BoneAnimation
{
	vector<Keyframe> Keyframes;										// TimePos 오름차순

	float GetStartTime() const { return Keyframes.front().TimePos; }
	float GetEndTime() const { return Keyframes.back().TimePos; }

	void Interpolate(float t, XMFLOAT4X4& M) const
	{
		const Keyframe* k0;
		const Keyframe* k1;
		float s = 0.0f;

		if (t <= GetStartTime())		{ k0 = k1 = &Keyframes.front(); }
		else if (t >= GetEndTime())		{ k0 = k1 = &Keyframes.back(); }
		else
		{
			size_t i = 0;
			while(t > Keyframes[i + 1].TimePos) ++i;				// t가 속한 구간 [i, i+1]
			k0 = &Keyframes[i];
			k1 = &Keyframes[i+1];
			s = (t - k0->TimePos) / (k1->TimePos - k0->TimePos);
		}

		XMVECTOR S = XMVectorLerp(XMLoadFloat3(&k0->Scale),					XMLoadFloat3(&k1->Scale), s);
		XMVECTOR P = XMVectorLerp(XMLoadFloat3(&k0->Translation),			XMLoadFloat3(&k1->Translation), s);
		XMVECTOR Q = XMQuaternionSlerp(XMLoadFloat4(&k0->RotationQuat),		XMLoadFloat4(&k1->RotationQuat), s);

		XMVECTOR origin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, origin, Q, P));				// S * R * T
	}
};