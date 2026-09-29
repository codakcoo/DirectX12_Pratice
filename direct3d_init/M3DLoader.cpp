#include "M3DLoader.h"
#include <fstream>


bool M3DLoader::LoadM3d(const string& filename, 
                        vector<SkinnedVertex>& vertices, 
                        vector<uint16_t>& indices, 
                        vector<M3dSubset>& subsets, 
                        vector<M3dMaterial>& mats, 
                        SkinnedData& skinInfo)
{
    ifstream fin(filename);
    if(!fin) return false;

    string ignore;
    uint32_t numMaterials = 0, numVertices = 0, numTriangles = 0, numBones = 0, numClips = 0;

    // ---- 헤더 ----
    fin >> ignore;                                  // ***m3d-File-Header***
    fin >> ignore >> numMaterials;
    fin >> ignore >> numVertices;
    fin >> ignore >> numTriangles;
    fin >> ignore >> numBones;
    fin >> ignore >> numClips;

    // ---- 재질: 판마다 필드 구성이 달라서 키워드로 찾음 ----
    fin >> ignore;                                  // ***Materials***
    mats.resize(numMaterials);
    for (auto& m : mats)
    {
        string key;
        while (fin >> key)
        {
            if (key == "Name:")                 fin >> m.Name;
            else if (key == "DiffuseMap:")      fin >> m.DiffuseMapName;
            else if (key == "NormalMap:")       { fin >> m.NormalMapName; break; }      // 재질 블록의 마지막 필드
        }
    }

    // ---- 서브셋 (재질 수와 같음) ----
    fin >> ignore;                                  // ***SubsetTable***
    subsets.resize(numMaterials);
    for (auto& s : subsets)
        fin >> ignore >> s.Id 
            >> ignore >> s.VertexStart 
            >> ignore >> s.VertexCount 
            >> ignore >> s.FaceStart 
            >> ignore >> s.FaceCount;

    // ---- 정점 ----
    fin >> ignore;
    vertices.resize(numVertices);
    for (auto& v : vertices)
    {
        float tw, w[4];
        int bi[4];
        fin >> ignore >> v.Pos.x >> v.Pos.y >> v.Pos.z;                                 // Position
        fin >> ignore >> v.TangentU.x >> v.TangentU.y >> v.TangentU.z >> tw;            // Tangent; (w버림)
        fin >> ignore >> v.Normal.x >> v.Normal.y >> v.Normal.z;                        // Normal
        fin >> ignore >> v.TexC.x >> v.TexC.y;                                          // Tex-Coords
        fin >> ignore >> w[0] >> w[1] >> w[2] >> w[3];                                  // BlendWeights
        fin >> ignore >> bi[0] >> bi[1] >> bi[2] >> bi[3];                              // BlendIndices

        v.BoneWeights = { w[0], w[1], w[2] };
        for (int k = 0; k < 4; ++k) v.BoneIndices[k] = (uint8_t)bi[k];
    }

    // ---- 삼각형 (전역 인덱스) ----
    fin >> ignore;                                  // ***Triangles***
    indices.resize(numTriangles * 3);
    for (auto& i : indices) fin >> i;

    // ---- 본 오프셋 (역바이드) ----
    fin >> ignore;                                  // ***BoneOffsets***
    vector<XMFLOAT4X4> boneOffsets(numBones);
    for (auto& m : boneOffsets)
    {
        fin >> ignore;                              // BoneOffset0
        fin >> m._11 >> m._12 >> m._13 >> m._14
            >> m._21 >> m._22 >> m._23 >> m._24
            >> m._31 >> m._32 >> m._33 >> m._34
            >> m._41 >> m._42 >> m._43 >> m._44;
    }

    // ---- 본 계층 ----
    fin >> ignore;                                  // ***BoneHierarchy***
    vector<int> bonehierarchy(numBones);
    for (auto& p : bonehierarchy)
        fin >> ignore >> p;                         // ParentIndexOfBone0: -1

    // ---- 애니메이션 클립 ----
    fin >> ignore;                                  // ***AnimationClips***
    unordered_map<string, AnimationClip> clips;
    for (uint32_t c = 0; c < numClips; ++c)
    {
        string clipName;
        fin >> ignore >> clipName;                  // AnimationClip Take1
        fin >> ignore;                              // {

        AnimationClip clip;
        clip.BoneAnimations.resize(numBones);
        for (auto& ba : clip.BoneAnimations)
        {
            string tok;
            while (fin >> tok && tok != "#Keyframes:") {}               // "Bone0 #Keyframes: 58"
            uint32_t numKeys = 0;
            fin >> numKeys;
            fin >> ignore;                          // {

            ba.Keyframes.resize(numKeys);
            for (auto& k : ba.Keyframes)
            {
                fin >> ignore >> k.TimePos
                    >> ignore >> k.Translation.x >> k.Translation.y >> k.Translation.z
                    >> ignore >> k.Scale.x >> k.Scale.y >> k.Scale.z
                    >> ignore >> k.RotationQuat.x >> k.RotationQuat.y >> k.RotationQuat.z >> k.RotationQuat.w;
            }
            fin >> ignore;                          // }
        }
        fin >> ignore;                              // }
        clips[clipName] = std::move(clip);
    }

    skinInfo.Set(bonehierarchy, boneOffsets, clips);

    // 본 오프셋 / 계층 / 클립은 2단계에서
    return !fin.fail();
}
