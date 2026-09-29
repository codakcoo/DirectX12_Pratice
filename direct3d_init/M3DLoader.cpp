#include "M3DLoader.h"
#include <fstream>


bool M3DLoader::LoadM3d(const string& filename, vector<SkinnedVertex>& vertices, vector<uint16_t>& indices, vector<M3dSubset>& subsets, vector<M3dMaterial>& mats)
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

    // 본 오프셋 / 계층 / 클립은 2단계에서
    return !fin.fail();
}
