//----------------------------------------------------------------------------------
// File:        Scene.h
// SDK Version: 2.0
// Email:       vrsupport@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------------


#pragma once

#include "assimp/scene.h"
#include "framework.h"
#include "D3DHelper.h"
#include "d3d11.h"
#include <vector>
#include <map>
#include <ppl.h>

using namespace util;

struct Vertex
{
	float3	m_pos;
	float2	m_texcoord;
	uint	m_normal;
	uint	m_tangent;
	uint	m_bitangent;
};

struct Plane
{
	float3 normal;
	float distance;

	Plane() : normal(), distance(0.f) { }
	Plane(const Plane &p) : normal(p.normal), distance(p.distance) { }
	Plane(float x, float y, float z, float d) : normal(makefloat3(x, y, z)), distance(d) { }

	float4 plane() const { return makefloat4(normal, distance); }

	void normalize()
	{
		float lengthSq = dot(normal, normal);
		float scale = (lengthSq > std::numeric_limits<float>::epsilon() ? (1.0f / sqrtf(lengthSq)) : 0);
		normal *= scale;
		distance *= scale;
	}
};

struct Frustum
{
	enum FrustumPlanes
	{
		NEAR_PLANE = 0,
		FAR_PLANE,
		LEFT_PLANE,
		RIGHT_PLANE,
		TOP_PLANE,
		BOTTOM_PLANE,
		PLANES_COUNT
	};

	Plane planes[PLANES_COUNT];

	Frustum() { }
	Frustum(const Frustum &f)
	{
		planes[0] = f.planes[0];
		planes[1] = f.planes[1];
		planes[2] = f.planes[2];
		planes[3] = f.planes[3];
		planes[4] = f.planes[4];
		planes[5] = f.planes[5];
	}
	Frustum(const float4x4 &viewProjMatrix)
	{
		planes[NEAR_PLANE] = Plane(-viewProjMatrix[0].z, -viewProjMatrix[1].z, -viewProjMatrix[2].z, viewProjMatrix[3].z);
		planes[FAR_PLANE] = Plane(-viewProjMatrix[0].w + viewProjMatrix[0].z, -viewProjMatrix[1].w + viewProjMatrix[1].z, -viewProjMatrix[2].w + viewProjMatrix[2].z, viewProjMatrix[3].w - viewProjMatrix[3].z);

		planes[LEFT_PLANE] = Plane(-viewProjMatrix[0].w - viewProjMatrix[0].x, -viewProjMatrix[1].w - viewProjMatrix[1].x, -viewProjMatrix[2].w - viewProjMatrix[2].x, viewProjMatrix[3].w + viewProjMatrix[3].x);
		planes[RIGHT_PLANE] = Plane(-viewProjMatrix[0].w + viewProjMatrix[0].x, -viewProjMatrix[1].w + viewProjMatrix[1].x, -viewProjMatrix[2].w + viewProjMatrix[2].x, viewProjMatrix[3].w - viewProjMatrix[3].x);

		planes[TOP_PLANE] = Plane(-viewProjMatrix[0].w + viewProjMatrix[0].y, -viewProjMatrix[1].w + viewProjMatrix[1].y, -viewProjMatrix[2].w + viewProjMatrix[2].y, viewProjMatrix[3].w - viewProjMatrix[3].y);
		planes[BOTTOM_PLANE] = Plane(-viewProjMatrix[0].w - viewProjMatrix[0].y, -viewProjMatrix[1].w - viewProjMatrix[1].y, -viewProjMatrix[2].w - viewProjMatrix[2].y, viewProjMatrix[3].w + viewProjMatrix[3].y);

		planes[0].normalize();
		planes[1].normalize();
		planes[2].normalize();
		planes[3].normalize();
		planes[4].normalize();
		planes[5].normalize();
	}

	bool intersectsWith(const float3 &point) const
	{
		for (int i = 0; i < PLANES_COUNT; ++i)
		{
			float distance = planes[i].normal.x * point.x + planes[i].normal.y * point.y + planes[i].normal.z * point.z;
			if (distance > planes[i].distance) return false;
		}

		return true;
	}

	bool intersectsWith(const box3 &box) const
	{
		float3 minPt;

		for (int i = 0; i < PLANES_COUNT; ++i)
		{
			minPt.x = planes[i].normal.x > 0 ? box.m_mins.x : box.m_maxs.x;
			minPt.y = planes[i].normal.y > 0 ? box.m_mins.y : box.m_maxs.y;
			minPt.z = planes[i].normal.z > 0 ? box.m_mins.z : box.m_maxs.z;

			float distance = planes[i].normal.x * minPt.x + planes[i].normal.y * minPt.y + planes[i].normal.z * minPt.z;
			if (distance > planes[i].distance) return false;
		}

		return true;
	}

};

struct Texture2DEx : public Texture2D
{
	int originalBPP;

	Texture2DEx()
		: Texture2D()
		, originalBPP(0)
	{ }
};

struct Material
{
	Texture2DEx* m_DiffuseTexture;
	Texture2DEx* m_SpecularTexture;
	Texture2DEx* m_NormalsTexture;
	Texture2DEx* m_EmissiveTexture;
	bool m_AlphaTested;
	float3 m_DiffuseColor;

	Material()
		: m_AlphaTested(false)
		, m_DiffuseTexture(nullptr)
		, m_SpecularTexture(nullptr)
		, m_NormalsTexture(nullptr)
		, m_EmissiveTexture(nullptr)
	{
	}
};

class Scene
{
public:
	struct LoadingStats
	{
		uint ObjectsTotal;
		uint ObjectsLoaded;
		uint TexturesTotal;
		uint TexturesLoaded;
	};

protected:
	ID3D11Device*           m_pDevice;
	aiScene*                m_pScene;

	std::vector<std::vector<box3>> m_SceneMeshInstanceBounds;
	box3                    m_SceneBounds;
	bool					m_SingleInstanceBuffer;

	std::string             m_ScenePath;

	Buffer                  m_IndexBuffer;
	Buffer                  m_VertexBuffer;
	std::vector<Buffer>		m_InstanceBuffers;
	std::vector<UINT>		m_CulledInstanceCounts;

	std::vector<UINT>       m_IndexOffsets;
	std::vector<UINT>       m_VertexOffsets;
	std::vector<Material>   m_Materials;
	std::vector<float4x4>	m_InstanceMatrices;

	std::vector<UINT>       m_MeshToSceneMapping;
	std::map<std::string, Texture2DEx*> m_LoadedTextures;

	Texture2DEx* LoadTextureFromFileAsync(const char* name, bool sRGB, LoadingStats& stats, concurrency::task_group& taskGroup);

public:
	Scene()
		: m_pScene(NULL)
		, m_pDevice(NULL)
		, m_SingleInstanceBuffer(false)
	{}
	virtual ~Scene()
	{
		Release();
		ReleaseResources();
	}


	HRESULT Load(const char* fileName, UINT flags = 0);
	HRESULT InitResources(ID3D11Device* pDevice, LoadingStats& stats, concurrency::task_group& taskGroup);
	void FinalizeInit(ID3D11DeviceContext* pContext);
	void UpdateBounds();
	void Release();
	void ReleaseResources();

	const char* GetScenePath() { return m_ScenePath.c_str(); }

	UINT GetMeshesNum() const { return m_pScene ? m_pScene->mNumMeshes : 0; }

	void FrustumCull(const Frustum& frustum, ID3D11DeviceContext* pCtx, bool enableCulling);

	ID3D11Buffer* GetIndexBuffer(UINT meshID, OUT UINT &offset) const;
	ID3D11Buffer* GetVertexBuffer(UINT meshID, OUT UINT &offset) const;

	void AddInstance(const float4x4& matrix) { m_InstanceMatrices.push_back(matrix); }
	UINT GetCulledInstancesNum(UINT meshID) const { return m_CulledInstanceCounts[meshID]; }
	bool IsSingleInstanceBuffer() const { return m_SingleInstanceBuffer; }
	ID3D11ShaderResourceView* GetInstanceBuffer(UINT meshID) const { return m_InstanceBuffers[m_SingleInstanceBuffer ? 0 : meshID].pSRV; }

	const Material* GetMaterial(UINT meshID) const;

	UINT GetMeshIndicesNum(UINT meshID) const;
};