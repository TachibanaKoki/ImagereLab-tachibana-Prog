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

HRESULT Scene::Load(const char* fileName, uint flags)
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

	uint facesNum = 0;
	uint verticesNum = 0;

	for (uint i = 0; i < m_pScene->mNumMeshes; ++i)
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

		for (uint sceneMesh = 0; sceneMesh < m_pScene->mNumMeshes; sceneMesh++)
		{
			point3 meshMin = _minBoundary;
			point3 meshMax = _maxBoundary;

			for (uint v = 0; v < m_pScene->mMeshes[sceneMesh]->mNumVertices; ++v)
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

			for (uint instance = 0; instance < m_InstanceMatrices.size(); instance++)
			{
				point3 instanceMin = _minBoundary;
				point3 instanceMax = _maxBoundary;

				for (uint v = 0; v < 8; v++)
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

uint GetMipLevelsNum(uint width, uint height)
{
	uint size = __min(width, height);
	uint levelsNum = (uint)(logf((float)size) / logf(2.0f)) + 1;

	return levelsNum;
}

Texture2DEx* Scene::LoadTextureFromFileAsync(const char* name, bool sRGB, LoadingStats& stats, concurrency::task_group& taskGroup)
{
	std::string str_path = GetScenePath();
	size_t pos = str_path.find_last_of("\\/");
	str_path = pos ? str_path.substr(0, pos) : "";
	str_path += '\\';
	str_path += name;

	std::string name_copy = name;

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

	taskGroup.run([this, sRGB, &stats, texture, str_path, name_copy]() 
	{
		FREE_IMAGE_FORMAT imageFormat = FreeImage_GetFileType(str_path.c_str());

		FIBITMAP* pBitmap = FreeImage_Load(imageFormat, str_path.c_str());
		if (pBitmap)
		{
			uint width = FreeImage_GetWidth(pBitmap);
			uint height = FreeImage_GetHeight(pBitmap);
			uint bpp = FreeImage_GetBPP(pBitmap);
			texture->originalBPP = bpp;

			NVRHI::Format::Enum formatRHI = NVRHI::Format::UNKNOWN;

			switch (bpp)
			{
			case 8:
				formatRHI = NVRHI::Format::R8_UNORM;
				break;

			case 24:
			{
				FIBITMAP* newBitmap = FreeImage_ConvertTo32Bits(pBitmap);
				FreeImage_Unload(pBitmap);
				pBitmap = newBitmap;
				formatRHI = sRGB ? NVRHI::Format::SBGRA8_UNORM : NVRHI::Format::BGRA8_UNORM;
				break;
			}

			case 32:
				formatRHI = sRGB ? NVRHI::Format::SBGRA8_UNORM : NVRHI::Format::BGRA8_UNORM;
				break;

			default:
				FreeImage_Unload(pBitmap);
				return;
			}

			NVRHI::TextureDesc textureDesc;
			textureDesc.width = width;
			textureDesc.height = height;
			textureDesc.format = formatRHI;
			textureDesc.mipLevels = GetMipLevelsNum(width, height);
			textureDesc.debugName = name_copy.c_str();
			NVRHI::TextureHandle textureHandle = m_rendererInterface->createTexture(textureDesc, nullptr);
			texture->Texture = textureHandle;

			texture->mipData[0] = pBitmap;

			for (uint mipLevel = 1; mipLevel < textureDesc.mipLevels; mipLevel++)
			{
				width /= 2;
				height /= 2;
				texture->mipData[mipLevel] = FreeImage_Rescale(texture->mipData[mipLevel - 1], width, height, FILTER_BILINEAR);
			}

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

HRESULT CreateStaticBuffer1(NVRHI::IRendererInterface* rendererInterface, NVRHI::BufferHandle& buffer, uint size, bool isVertexBuffer, void* pData)
{
	NVRHI::BufferDesc desc;
	desc.isVertexBuffer = isVertexBuffer;
	desc.isIndexBuffer = !isVertexBuffer;
	desc.byteSize = size;
	buffer = rendererInterface->createBuffer(desc, pData);
	return 0;
}

uint EncodeVectorAsRGBA8S(aiVector3D v)
{
	float3 vv = normalize(makefloat3(v.x, v.y, v.z));
	uint x = uint(vv.x * 127.0f);
	uint y = uint(vv.y * 127.0f);
	uint z = uint(vv.z * 127.0f);
	return (x & 0xff) | ((y & 0xff) << 8) | ((z & 0xff) << 16);
}

HRESULT Scene::InitResources(NVRHI::IRendererInterface* rendererInterface, LoadingStats& stats, concurrency::task_group& taskGroup)
{	
	m_rendererInterface = rendererInterface;
	
	if (!m_pScene)
		return E_FAIL;

	if (m_pScene->mNumMeshes)
	{
		std::vector<std::pair<uint, uint>> meshMaterials(m_pScene->mNumMeshes);
		for (uint sceneMesh = 0; sceneMesh < m_pScene->mNumMeshes; ++sceneMesh)
		{
			meshMaterials[sceneMesh] = std::pair<uint, uint>(sceneMesh, m_pScene->mMeshes[sceneMesh]->mMaterialIndex);
		}

		std::sort(meshMaterials.begin(), meshMaterials.end(), [](std::pair<uint, uint> a, std::pair<uint, uint> b) { return a.second < b.second; });

		m_IndexOffsets.resize(m_pScene->mNumMeshes);
		m_VertexOffsets.resize(m_pScene->mNumMeshes);

		uint totalIndices = 0;
		uint totalVertices = 0;

		// Count all the indices and vertices first
		for (uint meshID = 0; meshID < m_pScene->mNumMeshes; ++meshID)
		{
			m_IndexOffsets[meshID] = totalIndices;
			m_VertexOffsets[meshID] = totalVertices;
			uint sceneMesh = meshMaterials[meshID].first;

			totalIndices += m_pScene->mMeshes[sceneMesh]->mNumFaces * 3;
			totalVertices += m_pScene->mMeshes[sceneMesh]->mNumVertices;
		}

		//printf("%d meshes, %d indices, %d vertices\n", m_pScene->mNumMeshes, totalIndices, totalVertices);

		// Create buffer images
		m_Indices.resize(totalIndices);
		m_Vertices.resize(totalVertices);

		m_MeshToSceneMapping.resize(m_pScene->mNumMeshes);

		// Copy data into buffer images
		for (uint meshID = 0; meshID < m_pScene->mNumMeshes; ++meshID)
		{
			m_MeshToSceneMapping[meshID] = meshMaterials[meshID].first;

			uint sceneMesh = meshMaterials[meshID].first;
			uint indexOffset = m_IndexOffsets[meshID];
			uint vertexOffset = m_VertexOffsets[meshID];

			const aiMesh* pMesh = m_pScene->mMeshes[sceneMesh];
			
			// Indices
			for (uint f = 0; f < pMesh->mNumFaces; ++f)
			{
				memcpy(&m_Indices[indexOffset + f * 3], pMesh->mFaces[f].mIndices, sizeof(int) * 3);
			}


			for (unsigned int nVertex = 0; nVertex < pMesh->mNumVertices; nVertex++)
			{
				Vertex* pVertex = &m_Vertices[vertexOffset + nVertex];
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
		NVRHI::BufferDesc desc;
		desc.isIndexBuffer = true;
		desc.byteSize = totalIndices * sizeof(int);
		m_IndexBuffer_rhi = m_rendererInterface->createBuffer(desc, nullptr);

		desc.isIndexBuffer = false;
		desc.isVertexBuffer = true;
		desc.byteSize = totalVertices * sizeof(Vertex);
		m_VertexBuffer_rhi = m_rendererInterface->createBuffer(desc, nullptr);
	}

	if (m_pScene->HasMaterials())
	{
		m_Materials.resize(m_pScene->mNumMaterials);

		for (uint materialIndex = 0; materialIndex < m_pScene->mNumMaterials; ++materialIndex)
		{
			aiString texturePath;
			aiMaterial* material = m_pScene->mMaterials[materialIndex];
			Material* sceneMaterial = &m_Materials[materialIndex];

			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
			{
				sceneMaterial->m_DiffuseTexture = LoadTextureFromFileAsync(texturePath.C_Str(), true, stats, taskGroup);
			}

            /* The sample doesn't use specular maps, so there's need to load them.
			if (material->GetTexture(aiTextureType_SPECULAR, 0, &texturePath) == AI_SUCCESS)
			{
				sceneMaterial->m_SpecularTexture = LoadTextureFromFileAsync(texturePath.C_Str(), false, stats, taskGroup);
			}*/

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
	uint instanceBuffersNum = m_SingleInstanceBuffer ? 1 : GetMeshesNum();

	m_InstanceBuffers_rhi.resize(instanceBuffersNum);
	for (uint bufferID = 0; bufferID < instanceBuffersNum; bufferID++)
	{
		NVRHI::BufferDesc bufferDesc;
		bufferDesc.byteSize = uint(sizeof(float4x4) * m_InstanceMatrices.size());
		bufferDesc.structStride = sizeof(float4x4);
		NVRHI::BufferHandle buffer = m_rendererInterface->createBuffer(bufferDesc, nullptr);
		m_InstanceBuffers_rhi[bufferID] = buffer;
	}

	return S_OK;
}

void Scene::FinalizeInit()
{
	m_rendererInterface->writeBuffer(m_IndexBuffer_rhi, &m_Indices[0], m_Indices.size() * sizeof(m_Indices[0]));
	m_rendererInterface->writeBuffer(m_VertexBuffer_rhi, &m_Vertices[0], m_Vertices.size() * sizeof(m_Vertices[0]));

	if (m_SingleInstanceBuffer)
	{
		m_rendererInterface->writeBuffer(m_InstanceBuffers_rhi[0], &m_InstanceMatrices[0], m_InstanceMatrices.size() * sizeof(m_InstanceMatrices[0]));
	}

	for (auto it : m_LoadedTextures)
	{
		if (it.second->Texture)
		{
			NVRHI::TextureDesc textureDesc = m_rendererInterface->describeTexture(it.second->Texture);

			for (uint mipLevel = 0; mipLevel < textureDesc.mipLevels; mipLevel++)
			{
				FIBITMAP* mipData = it.second->mipData[mipLevel];
				void* pData = FreeImage_GetBits(mipData);
				uint rowPitch = FreeImage_GetPitch(mipData);

				m_rendererInterface->writeTexture(it.second->Texture, mipLevel, pData, rowPitch, 0);

				FreeImage_Unload(mipData);
				it.second->mipData[mipLevel] = nullptr;
			}
		}
        else
        {
            // If the texture failed to load, unbind it from all materials.
            for (auto& material : m_Materials)
            {
                if (material.m_DiffuseTexture == it.second) 
                    material.m_DiffuseTexture = nullptr;
                if (material.m_EmissiveTexture == it.second) 
                    material.m_EmissiveTexture = nullptr;
                if (material.m_NormalsTexture == it.second) 
                    material.m_NormalsTexture = nullptr;
                if (material.m_SpecularTexture == it.second) 
                    material.m_SpecularTexture = nullptr;
            }
        }
	}

	for(auto& material : m_Materials)
	{
		if (material.m_DiffuseTexture && material.m_DiffuseTexture->Texture && material.m_DiffuseTexture->originalBPP == 32)
			material.m_AlphaTested = true;
	}
}

void Scene::ReleaseResources()
{
	if (!m_rendererInterface)
		return;

	m_rendererInterface->destroyBuffer(m_IndexBuffer_rhi);
	m_rendererInterface->destroyBuffer(m_VertexBuffer_rhi);

	for (auto it : m_LoadedTextures)
	{
		m_rendererInterface->destroyTexture(it.second->Texture);
		delete it.second;
	}
	m_LoadedTextures.clear();

	for (auto it : m_InstanceBuffers_rhi)
	{
		m_rendererInterface->destroyBuffer(it);
	}
	m_InstanceBuffers_rhi.clear();
}

NVRHI::BufferHandle Scene::GetIndexBuffer_rhi(uint meshID, OUT uint &offset) const
{
	if (meshID >= m_IndexOffsets.size()) return NULL;

	offset = m_IndexOffsets[meshID];
	return m_IndexBuffer_rhi;
}

NVRHI::BufferHandle Scene::GetVertexBuffer_rhi(uint meshID, OUT uint &offset) const
{
	if (meshID >= m_VertexOffsets.size()) return NULL;

	offset = m_VertexOffsets[meshID];
	return m_VertexBuffer_rhi;
}

const Material* Scene::GetMaterial(uint meshID) const
{
	if (m_pScene->HasMaterials() && meshID < m_MeshToSceneMapping.size())
	{
		uint materialIndex = m_pScene->mMeshes[m_MeshToSceneMapping[meshID]]->mMaterialIndex;
		return &m_Materials[materialIndex];
	}

	return NULL;
}

uint Scene::GetMeshIndicesNum(uint meshID) const
{
	if (m_pScene && meshID < m_pScene->mNumMeshes)
	{
		return m_pScene->mMeshes[m_MeshToSceneMapping[meshID]]->mNumFaces * 3;
	}

	return 0;
}

void Scene::FrustumCull(const Frustum& frustum, bool enableCulling)
{
	const uint numMeshes = GetMeshesNum();
	const uint numInstances = uint(m_InstanceMatrices.size());

	if(m_SingleInstanceBuffer)
	{ 
		for (uint meshID = 0; meshID < numMeshes; meshID++)
		{
			uint sceneMesh = m_MeshToSceneMapping[meshID];

			m_CulledInstanceCounts[meshID] = (!enableCulling || frustum.intersectsWith(m_SceneMeshInstanceBounds[sceneMesh][0])) ? 1 : 0;
		}
	}
	else
	{
		std::vector<float4x4> matrices;
		matrices.resize(numInstances);

		for (uint meshID = 0; meshID < numMeshes; meshID++)
		{
			uint sceneMesh = m_MeshToSceneMapping[meshID];
			uint culledInstancesNum = 0;
			if (enableCulling)
			{
				for (uint instanceID = 0; instanceID < numInstances; instanceID++)
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
				m_rendererInterface->writeBuffer(m_InstanceBuffers_rhi[meshID], &matrices[0], sizeof(float4x4) * numInstances);
			}
		}
	}
}
;