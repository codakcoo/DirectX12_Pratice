#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <DirectXMath.h>
#include "Animation.h"

using namespace std;
using namespace DirectX;

// 앞 4개 멤버는 Vertex와 같은 순서 -> 기존 입력 레이아웃 오프셋 그대로 유효

struct SkinnedVertex
{
	XMFLOAT3	Pos;
	XMFLOAT3	Normal;
	XMFLOAT2	TexC;
	XMFLOAT3	TangentU;
	XMFLOAT3	BoneWeights;					// 4번째 가중치 = 1 - (w0 + w1 + w2)
	uint8_t		BoneIndices[4];
};

struct M3dSubset
{
	uint32_t Id = 0;
	uint32_t VertexStart = 0, VertexCount = 0;
	uint32_t FaceStart = 0, FaceCount = 0;
};

struct M3dMaterial
{
	string Name;
	string DiffuseMapName;
	string NormalMapName;
};

class M3DLoader
{
public:
	static bool LoadM3d(const string& filename,
		vector<SkinnedVertex>& vertices,
		vector<uint16_t>& indices,
		vector<M3dSubset>& subsets,
		vector<M3dMaterial>& mats,
		SkinnedData& skinInfo);
};

