//----------------------------------------------------------------------------------
// File:        Scene.cpp
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


#include "Scene.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "FreeImage.h"
#include <assert.h>

HRESULT Scene::Load(const char* fileName, UINT flags)
{
	flags |= aiProcess_Triangulate;
	flags |= aiProcess_CalcTangentSpace;
	flags |= aiProcess_GenNormals;

	m_pScene = (aiScene*)aiImportFile(fileName, flags);

	if (!m_pScene)
	{
		printf("unable to load scene file `%s`: %s\n", fileName, aiGetErrorString());
		return E_FAIL;
	}

	m_ScenePath = fileName;

	UINT facesNum = 0;
	UINT verticesNum = 0;

	for (UINT i = 0; i < m_pScene->mNumMeshes; ++i)
	{
		facesNum += m_pScene->mMeshes[i]->mNumFaces;
		verticesNum += m_pScene->mMeshes[i]->mNumVertices;
	}

	return S_OK;
}

void Scene::Release()
{
	printf("Releasing the scene...\n");

	aiReleaseImport(m_pScene);
	m_pScene = NULL;
}

void Scene::UpdateBounds()
{
	assert(m_pScene != NULL);

	const float maxFloat = 3.402823466e+38F;
	point3 _minBoundary = makepoint3(maxFloat);
	point3 _maxBoundary = makepoint3(-maxFloat);

	if (m_pScene->mNumMeshes)
	{
		m_SceneMeshInstanceBounds.resize(m_pScene->mNumMeshes);
		m_CulledInstanceCounts.resize(m_pScene->mNumMeshes);

		m_SceneBounds.m_mins = _minBoundary;
		m_SceneBounds.m_maxs = _maxBoundary;

		for (UINT sceneMesh = 0; sceneMesh < m_pScene->mNumMeshes; sceneMesh++)
		{
			point3 meshMin = _minBoundary;
			point3 meshMax = _maxBoundary;

			for (UINT v = 0; v < m_pScene->mMeshes[sceneMesh]->mNumVertices; ++v)
			{
				meshMin.x = __min(meshMin.x, m_pScene->mMeshes[sceneMesh]->mVertices[v].x);
				meshMin.y = __min(meshMin.y, m_pScene->mMeshes[sceneMesh]->mVertices[v].y);
				meshMin.z = __min(meshMin.z, m_pScene->mMeshes[sceneMesh]->mVertices[v].z);

				meshMax.x = __max(meshMax.x, m_pScene->mMeshes[sceneMesh]->mVertices[v].x);
				meshMax.y = __max(meshMax.y, m_pScene->mMeshes[sceneMesh]->mVertices[v].y);
				meshMax.z = __max(meshMax.z, m_pScene->mMeshes[sceneMesh]->mVertices[v].z);
			}

			const point3 bboxVertices[] = {
				makepoint3(meshMin.x, meshMin.y, meshMin.z),
				makepoint3(meshMin.x, meshMin.y, meshMax.z),
				makepoint3(meshMin.x, meshMax.y, meshMin.z),
				makepoint3(meshMin.x, meshMax.y, meshMax.z),
				makepoint3(meshMax.x, meshMin.y, meshMin.z),
				makepoint3(meshMax.x, meshMin.y, meshMax.z),
				makepoint3(meshMax.x, meshMax.y, meshMin.z),
				makepoint3(meshMax.x, meshMax.y, meshMax.z),
			};

			m_SceneMeshInstanceBounds[sceneMesh].resize(m_InstanceMatrices.size());

			for (UINT instance = 0; instance < m_InstanceMatrices.size(); instance++)
			{
				point3 instanceMin = _minBoundary;
				point3 instanceMax = _maxBoundary;

				for (UINT v = 0; v < 8; v++)
				{
					float4 transformedVertex = makefloat4(makefloat3(bboxVertices[v]), 1.0f) * m_InstanceMatrices[instance];
					transformedVertex /= transformedVertex.w;

					point3 transformedPt = makepoint3(transformedVertex);

					instanceMin = min(instanceMin, transformedPt);
					instanceMax = max(instanceMax, transformedPt);
				}

				m_SceneMeshInstanceBounds[sceneMesh][instance] = makebox3(instanceMin, instanceMax);

				m_SceneBounds.m_mins = min(m_SceneBounds.m_mins, instanceMin);
				m_SceneBounds.m_maxs = max(m_SceneBounds.m_maxs, instanceMax);
			}

		}
	}
}

UINT GetMipLevelsNum(UINT width, UINT height)
{
	UINT size = __min(width, height);
	UINT levelsNum = (UINT)(logf((float)size) / logf(2.0f)) + 1;

	return levelsNum;
}

Texture2DEx* Scene::LoadTextureFromFileAsync(const char* name, bool sRGB, LoadingStats& stats, concurrency::task_group& taskGroup)
{
	std::string str_path = GetScenePath();
	size_t pos = str_path.find_last_of("\\/");
	str_path = pos ? str_path.substr(0, pos) : "";
	str_path += '\\';
	str_path += name;

	// First see if this texture is already loaded (or being loaded).

	Texture2DEx* texture = m_LoadedTextures[str_path];
	if (texture)
	{
		return texture;
	}

	// Allocate a new texture slot for this file name and return it. Load the file later in a thread pool.
	// LoadTextureFromFileAsync function for a given scene is only called from one thread, so there is no 
	// chance of loading the same texture twice.

	texture = new Texture2DEx();
	m_LoadedTextures[str_path] = texture;
	InterlockedIncrement(&stats.TexturesTotal);

	taskGroup.run([this, sRGB, &stats, texture, str_path]() 
	{
		FREE_IMAGE_FORMAT imageFormat = FreeImage_GetFileType(str_path.c_str());

		FIBITMAP* pBitmap = FreeImage_Load(imageFormat, str_path.c_str());
		if (pBitmap)
		{
			UINT width = FreeImage_GetWidth(pBitmap);
			UINT height = FreeImage_GetHeight(pBitmap);
			UINT bpp = FreeImage_GetBPP(pBitmap);
			texture->originalBPP = bpp;

			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

			switch (bpp)
			{
			case 8:
				format = DXGI_FORMAT_R8_UNORM;
				break;

			case 24:
			{
				FIBITMAP* newBitmap = FreeImage_ConvertTo32Bits(pBitmap);
				FreeImage_Unload(pBitmap);
				pBitmap = newBitmap;
				format = sRGB ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
				break;
			}

			case 32:
				format = sRGB ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
				break;

			default:
				FreeImage_Unload(pBitmap);
				return;
			}

			CD3D11_TEXTURE2D_DESC desc(format, width, height, 1, GetMipLevelsNum(width, height), D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
			desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

			// Use the same data for all mip levels. Actual mips are generated on the GPU after all loading is finished.
			// This is a bit wasteful because extra data is uploaded to the GPU, but:
			//   - InitialData must contain data for all levels, there is no way to upload only level 0 in CreateTexture2D. 
			//     It is possible to compute actual mip data on the CPU as well, but the GPU can do it faster.
			//   - UpdateSubresource and Map require a Context, which can only be used from one thread.

			D3D11_SUBRESOURCE_DATA initialData[16] = {};
			for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++)
			{
				initialData[mipLevel] = D3D11_SUBRESOURCE_DATA{ FreeImage_GetBits(pBitmap), FreeImage_GetPitch(pBitmap), 0 };
			}

			if (SUCCEEDED(m_pDevice->CreateTexture2D(&desc, initialData, &texture->pResource)))
			{
				m_pDevice->CreateShaderResourceView(texture->pResource, NULL, &texture->pSRV);
			}

			FreeImage_Unload(pBitmap);

			InterlockedIncrement(&stats.TexturesLoaded);
		}
		else
		{
			char error[MAX_PATH + 50];
			sprintf_s(error, "Couldn't load texture file `%s`\n", str_path.c_str());
			OutputDebugStringA(error);
		}
	});

	return texture;
}

HRESULT CreateStaticBuffer(ID3D11Device* device, Buffer& buffer, UINT size, UINT bindFalgs, void* pData)
{
	D3D11_BUFFER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bindFalgs;

	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = pData;
	return device->CreateBuffer(&desc, &initialData, &buffer.pResource);
}

UINT EncodeVectorAsRGBA8S(aiVector3D v)
{
	float3 vv = normalize(makefloat3(v.x, v.y, v.z));
	UINT x = UINT(vv.x * 127.0f);
	UINT y = UINT(vv.y * 127.0f);
	UINT z = UINT(vv.z * 127.0f);
	return (x & 0xff) | ((y & 0xff) << 8) | ((z & 0xff) << 16);
}

HRESULT Scene::InitResources(ID3D11Device* pDevice, LoadingStats& stats, concurrency::task_group& taskGroup)
{
	m_pDevice = pDevice;
	m_pDevice->AddRef();

	if (!m_pScene)
		return E_FAIL;

	if (m_pScene->mNumMeshes)
	{
		std::vector<std::pair<UINT, UINT>> meshMaterials(m_pScene->mNumMeshes);
		for (UINT sceneMesh = 0; sceneMesh < m_pScene->mNumMeshes; ++sceneMesh)
		{
			meshMaterials[sceneMesh] = std::pair<UINT, UINT>(sceneMesh, m_pScene->mMeshes[sceneMesh]->mMaterialIndex);
		}

		std::sort(meshMaterials.begin(), meshMaterials.end(), [](std::pair<UINT, UINT> a, std::pair<UINT, UINT> b) { return a.second < b.second; });

		m_IndexOffsets.resize(m_pScene->mNumMeshes);
		m_VertexOffsets.resize(m_pScene->mNumMeshes);

		UINT totalIndices = 0;
		UINT totalVertices = 0;

		// Count all the indices and vertices first
		for (UINT meshID = 0; meshID < m_pScene->mNumMeshes; ++meshID)
		{
			m_IndexOffsets[meshID] = totalIndices;
			m_VertexOffsets[meshID] = totalVertices;
			UINT sceneMesh = meshMaterials[meshID].first;

			totalIndices += m_pScene->mMeshes[sceneMesh]->mNumFaces * 3;
			totalVertices += m_pScene->mMeshes[sceneMesh]->mNumVertices;
		}

		//printf("%d meshes, %d indices, %d vertices\n", m_pScene->mNumMeshes, totalIndices, totalVertices);

		// Create buffer images
		UINT* pIndices = new UINT[totalIndices];
		Vertex* pVertices = new Vertex[totalVertices];

		ZeroMemory(pVertices, totalVertices * sizeof(Vertex));

		m_MeshToSceneMapping.resize(m_pScene->mNumMeshes);

		// Copy data into buffer images
		for (UINT meshID = 0; meshID < m_pScene->mNumMeshes; ++meshID)
		{
			m_MeshToSceneMapping[meshID] = meshMaterials[meshID].first;

			UINT sceneMesh = meshMaterials[meshID].first;
			UINT indexOffset = m_IndexOffsets[meshID];
			UINT vertexOffset = m_VertexOffsets[meshID];

			const aiMesh* pMesh = m_pScene->mMeshes[sceneMesh];

			// Indices
			for (UINT f = 0; f < pMesh->mNumFaces; ++f)
			{
				memcpy(pIndices + indexOffset + f * 3, pMesh->mFaces[f].mIndices, sizeof(int) * 3);
			}


			for (unsigned int nVertex = 0; nVertex < pMesh->mNumVertices; nVertex++)
			{
				Vertex* pVertex = pVertices + vertexOffset + nVertex;
				pVertex->m_pos = *(float3*)&pMesh->mVertices[nVertex];

				if(pMesh->HasTextureCoords(0)) 
					pVertex->m_texcoord = *(float2*)&pMesh->mTextureCoords[0][nVertex];

				if(pMesh->HasNormals()) 
					pVertex->m_normal = EncodeVectorAsRGBA8S(pMesh->mNormals[nVertex]);

				if (pMesh->HasTangentsAndBitangents())
				{
					pVertex->m_tangent = EncodeVectorAsRGBA8S(pMesh->mTangents[nVertex]);
					pVertex->m_bitangent = EncodeVectorAsRGBA8S(pMesh->mBitangents[nVertex]);
				}
			}
		}

		// Create buffers
		if (FAILED(CreateStaticBuffer(m_pDevice, m_IndexBuffer, totalIndices * sizeof(int), D3D11_BIND_INDEX_BUFFER, pIndices))) return E_FAIL;
		if (FAILED(CreateStaticBuffer(m_pDevice, m_VertexBuffer, totalVertices * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER, pVertices))) return E_FAIL;

		// Delete buffer images
		delete[] pIndices;
		delete[] pVertices;
	}

	if (m_pScene->HasMaterials())
	{
		m_Materials.resize(m_pScene->mNumMaterials);

		for (UINT materialIndex = 0; materialIndex < m_pScene->mNumMaterials; ++materialIndex)
		{
			aiString texturePath;
			aiMaterial* material = m_pScene->mMaterials[materialIndex];
			Material* sceneMaterial = &m_Materials[materialIndex];

			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
			{
				sceneMaterial->m_DiffuseTexture = LoadTextureFromFileAsync(texturePath.C_Str(), true, stats, taskGroup);
			}

			if (material->GetTexture(aiTextureType_SPECULAR, 0, &texturePath) == AI_SUCCESS)
			{
				sceneMaterial->m_SpecularTexture = LoadTextureFromFileAsync(texturePath.C_Str(), false, stats, taskGroup);
			}

			if (material->GetTexture(aiTextureType_HEIGHT, 0, &texturePath) == AI_SUCCESS)
			{
				sceneMaterial->m_NormalsTexture = LoadTextureFromFileAsync(texturePath.C_Str(), false, stats, taskGroup);
			}

			if (material->GetTexture(aiTextureType_EMISSIVE, 0, &texturePath) == AI_SUCCESS)
			{
				sceneMaterial->m_EmissiveTexture = LoadTextureFromFileAsync(texturePath.C_Str(), false, stats, taskGroup);
			}

			aiColor3D color;
			if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
			{
				sceneMaterial->m_DiffuseColor = makefloat3(color.r, color.g, color.b);
			}
		}
	}

	// For a scene with single instance, use one instance matrix buffer for everything.
	// Frustum culling will not overwrite that buffer, it will only set the mesh instance counts to 0 or 1.
	m_SingleInstanceBuffer = (m_InstanceMatrices.size() == 1);
	UINT instanceBuffersNum = m_SingleInstanceBuffer ? 1 : GetMeshesNum();

	m_InstanceBuffers.resize(instanceBuffersNum);
	for (UINT bufferID = 0; bufferID < instanceBuffersNum; bufferID++)
	{
		D3D11_BUFFER_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.ByteWidth = UINT(sizeof(float4x4) * m_InstanceMatrices.size());
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(float4x4);
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initialData;
		initialData.pSysMem = &m_InstanceMatrices[0];
		pDevice->CreateBuffer(&desc, &initialData, &m_InstanceBuffers[bufferID].pResource);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = UINT(m_InstanceMatrices.size());
		pDevice->CreateShaderResourceView(m_InstanceBuffers[bufferID].pResource, &srvDesc, &m_InstanceBuffers[bufferID].pSRV);
	}

	return S_OK;
}

void Scene::FinalizeInit(ID3D11DeviceContext* pContext)
{
	for (auto it : m_LoadedTextures)
	{
		if (it.second && it.second->pSRV)
		{
			pContext->GenerateMips(it.second->pSRV);
		}
	}

	for(auto& material : m_Materials)
	{
		if (material.m_DiffuseTexture && material.m_DiffuseTexture->pSRV && material.m_DiffuseTexture->originalBPP == 32)
			material.m_AlphaTested = true;
	}
}

void Scene::ReleaseResources()
{
	m_IndexBuffer.Release();
	m_VertexBuffer.Release();
	m_Materials.clear();

	for (auto it : m_LoadedTextures)
		delete it.second;

	m_LoadedTextures.clear();

	SAFE_RELEASE(m_pDevice);
}

ID3D11Buffer* Scene::GetIndexBuffer(UINT meshID, OUT UINT &offset) const
{
	if (meshID >= m_IndexOffsets.size()) return NULL;

	offset = m_IndexOffsets[meshID];
	return m_IndexBuffer.pResource;
}

ID3D11Buffer* Scene::GetVertexBuffer(UINT meshID, OUT UINT &offset) const
{
	if (meshID >= m_VertexOffsets.size()) return NULL;

	offset = m_VertexOffsets[meshID];
	return m_VertexBuffer.pResource;
}

const Material* Scene::GetMaterial(UINT meshID) const
{
	if (m_pScene->HasMaterials() && meshID < m_MeshToSceneMapping.size())
	{
		UINT materialIndex = m_pScene->mMeshes[m_MeshToSceneMapping[meshID]]->mMaterialIndex;
		return &m_Materials[materialIndex];
	}

	return NULL;
}

UINT Scene::GetMeshIndicesNum(UINT meshID) const
{
	if (m_pScene && meshID < m_pScene->mNumMeshes)
	{
		return m_pScene->mMeshes[m_MeshToSceneMapping[meshID]]->mNumFaces * 3;
	}

	return 0;
}

void Scene::FrustumCull(const Frustum& frustum, ID3D11DeviceContext* pCtx, bool enableCulling)
{
	const UINT numMeshes = GetMeshesNum();
	const UINT numInstances = UINT(m_InstanceMatrices.size());

	if(m_SingleInstanceBuffer)
	{ 
		for (UINT meshID = 0; meshID < numMeshes; meshID++)
		{
			UINT sceneMesh = m_MeshToSceneMapping[meshID];

			m_CulledInstanceCounts[meshID] = (!enableCulling || frustum.intersectsWith(m_SceneMeshInstanceBounds[sceneMesh][0])) ? 1 : 0;
		}
	}
	else
	{
		std::vector<float4x4> matrices;
		matrices.resize(numInstances);

		for (UINT meshID = 0; meshID < numMeshes; meshID++)
		{
			UINT sceneMesh = m_MeshToSceneMapping[meshID];
			UINT culledInstancesNum = 0;
			if (enableCulling)
			{
				for (UINT instanceID = 0; instanceID < numInstances; instanceID++)
				{
					if (frustum.intersectsWith(m_SceneMeshInstanceBounds[sceneMesh][instanceID]))
					{
						matrices[culledInstancesNum] = m_InstanceMatrices[instanceID];
						culledInstancesNum++;
					}
				}
			}
			else
			{
				culledInstancesNum = numInstances;
				memcpy(&matrices[0], &m_InstanceMatrices[0], sizeof(float4x4) * numInstances);
			}

			m_CulledInstanceCounts[meshID] = culledInstancesNum;

			if (culledInstancesNum > 0)
			{
				pCtx->UpdateSubresource(m_InstanceBuffers[meshID].pResource, 0, nullptr, &matrices[0], 0, 0);
			}
		}
	}
}
