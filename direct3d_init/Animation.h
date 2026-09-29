#pragma once
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>			// std::max
#include <cstdint>				// uint32_t

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
	std::vector<Keyframe> Keyframes;										// TimePos 오름차순

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

struct AnimationClip
{
	std::vector<BoneAnimation> BoneAnimations;						// 본 i의 애니메이션

	float GetClipEndTime() const
	{
		float t = 0.0f;
		for (auto& b : BoneAnimations)
		{
			if(!b.Keyframes.empty()) t = (std::max)(t, b.GetEndTime());
		}
		return t;
	}

	void Interpolate(float t, std::vector<XMFLOAT4X4>& toParent) const
	{
		for (size_t i = 0; i < BoneAnimations.size(); ++i)
		{
			if (BoneAnimations[i].Keyframes.empty())
				XMStoreFloat4x4(&toParent[i], XMMatrixIdentity());
			else
				BoneAnimations[i].Interpolate(t, toParent[i]);
		}
	}
};

class SkinnedData
{
public:
	uint32_t  BoneCount() const { return (uint32_t)mBoneHierarchy.size(); }

	float GetClipEndTime(const std::string& clip) const { return mAnimations.at(clip).GetClipEndTime(); }

	void Set(std::vector<int>& hierarchy, std::vector<XMFLOAT4X4>& offsets, std::unordered_map<std::string, AnimationClip>& clips)
	{
		mBoneHierarchy = hierarchy;
		mBoneOffsets = offsets;
		mAnimations = clips;
	}

	void GetFinalTransforms(const std::string& clipName, float t, std::vector<XMFLOAT4X4>& finals) const
	{
		const uint32_t  n = BoneCount();
		std::vector<XMFLOAT4X4> toParent(n);
		mAnimations.at(clipName).Interpolate(t, toParent);

		// 부모 인덱스 < 자식 인덱스라서 앞에서부터 한 번에 누적 가능
		std::vector<XMFLOAT4X4> toRoot(n);
		toRoot[0] = toParent[0];
		for (uint32_t  i = 1; i < n; ++i)
		{
			XMMATRIX parentToRoot = XMLoadFloat4x4(&toRoot[mBoneHierarchy[i]]);
			XMStoreFloat4x4(&toRoot[i], XMLoadFloat4x4(&toParent[i]) * parentToRoot);
		}

		finals.resize(n);
		for (uint32_t  i = 0; i < n; ++i)
			XMStoreFloat4x4(&finals[i], XMLoadFloat4x4(&mBoneOffsets[i]) * XMLoadFloat4x4(&toRoot[i]));
	}

	std::string FirstClipName() const { return mAnimations.begin()->first; }

private:
	std::vector<int>								mBoneHierarchy;			// 부모 인덱스 (-1 = 루트)
	std::vector<XMFLOAT4X4>							mBoneOffsets;			// 역 바인드 행렬
	std::unordered_map<std::string, AnimationClip>	mAnimations;
};