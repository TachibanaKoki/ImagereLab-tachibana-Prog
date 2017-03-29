//----------------------------------------------------------------------------------
// File:        ShaderFactory.cpp
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

#include "ShaderFactory.h"
#include <Windows.h>
#include <d3dcompiler.h>
#include <sstream>

#pragma comment(lib, "d3dcompiler.lib")

ShaderCacheEntry::ShaderCacheEntry()
	: data(nullptr)
	, sizeOfData(0)
{
}

ShaderCacheEntry::~ShaderCacheEntry()
{
	delete[] data;
	data = NULL;
}

UINT ShaderFactory::Hasher(const std::string& input)
{
  UINT _Val = 2166136261U;
  UINT _First = 0;
  UINT _Last = (UINT)input.size();
  UINT _Stride = 1;// + _Last / 10;

  for (; _First < _Last; _First += _Stride)
    _Val = 16777619U * _Val ^ (UINT)input[_First];

  return _Val;
}

UINT ShaderFactory::GetNameHash(const std::string& fileName)
{
  return Hasher(fileName);
}

UINT ShaderFactory::GetDefinesHash(const std::vector<shaderMacro>* pDefines)
{
  size_t shaderDefinesNum = pDefines ? pDefines->size() : 0;

  std::stringstream hashKey;
  if (pDefines && shaderDefinesNum)
  {
    for (auto it = pDefines->begin(); it != pDefines->end(); ++it)
    {
      hashKey << it->Name << '=' << it->Definition;
    }
  }

  return Hasher(hashKey.str());
}


ShaderFactory::ShaderFactory(ID3D11Device* pDevice, const std::string& basePath)
	: m_pDevice(pDevice)
	, m_BasePath(basePath)
{
}

void ShaderFactory::ClearCache()
{
	m_BytecodeCache.clear();
}

bool ShaderFactory::GetBytecode(const char* fileName, const char* target, const std::vector<shaderMacro>* pDefines, ShaderByteCode& byteCode)
{
	(void)target;
	UINT nameHash = GetNameHash(fileName);
	UINT definesHash = GetDefinesHash(pDefines);

	UINT64 cacheKey = (UINT64(nameHash) << 32) | definesHash;
	ShaderCacheEntry& entry = m_BytecodeCache[cacheKey];

	if (entry.data)
	{
		byteCode.pBytecode = &(entry.data[0]);
		byteCode.bytecodeSize = entry.sizeOfData;

		return true;
	}
	else
	{
		std::string fullPath = m_BasePath + fileName + ".hlsl";
		std::wstring fullPathW(fullPath.begin(), fullPath.end());

		ID3DBlob* pCode = nullptr;
		ID3DBlob* pErrorMessages = nullptr;

		std::vector<D3D_SHADER_MACRO> definesWithTerminator;
		D3D_SHADER_MACRO* pD3DDefines = nullptr;

		if (pDefines)
		{
			definesWithTerminator = *pDefines;
			definesWithTerminator.push_back(shaderMacro{ nullptr, nullptr });
			pD3DDefines = &definesWithTerminator[0];
		}

		while (true)
		{
			HRESULT hr = D3DCompileFromFile(
				fullPathW.c_str(),
				pD3DDefines,
				D3D_COMPILE_STANDARD_FILE_INCLUDE,
				"main",
				target,
				D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG,
				0,
				&pCode,
				&pErrorMessages);

			if (SUCCEEDED(hr) && pCode)
			{
				if (pErrorMessages)
				{
					OutputDebugStringA((const char*)pErrorMessages->GetBufferPointer());
					pErrorMessages->Release();
				}

				break;
			}

			std::stringstream ss;
			ss << "ERROR: Failed to compile shader " << fileName << " for target " << target << ".\n";

			if (pErrorMessages)
			{
				const char* msg = (char*)pErrorMessages->GetBufferPointer();
				ss << msg << "\n";
				pErrorMessages->Release();
			}

			OutputDebugStringA(ss.str().c_str());

			ss << "\nReload and retry?";

			int mbResult = MessageBoxA(0, ss.str().c_str(), "VRWorks SDK", MB_ICONERROR | MB_YESNO);

			if (mbResult == IDNO)
				ExitProcess(1);
			else
				continue;
		}

		size_t size = pCode->GetBufferSize();
		char* copyOfData = new char[size];
		memcpy(copyOfData, pCode->GetBufferPointer(), size);
		pCode->Release();

		entry.data = copyOfData;
		entry.sizeOfData = size;

		byteCode.bytecodeSize = size;
		byteCode.pBytecode = copyOfData;

		return true;
	}
}

bool ShaderFactory::CreateVertexShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11VertexShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "vs_5_0", pDefines, byteCode))
		return false;

	if (FAILED(m_pDevice->CreateVertexShader(byteCode.pBytecode, byteCode.bytecodeSize, NULL, ppShader)))
		return false;

	return true;
}

bool ShaderFactory::CreateVertexShaderEx(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_VERTEX_SHADER_EX* pArgs, ID3D11VertexShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "vs_5_0", pDefines, byteCode))
		return false;

	NvAPI_Status Status = NvAPI_D3D11_CreateVertexShaderEx(m_pDevice, byteCode.pBytecode, byteCode.bytecodeSize, NULL, pArgs, ppShader);

	if (Status != NVAPI_OK)
		return false;

	return true;
}

bool ShaderFactory::CreateInputLayout(const char* fileName, const std::vector<shaderMacro>* pDefines, const D3D11_INPUT_ELEMENT_DESC* pElements, UINT numElements, ID3D11InputLayout** ppLayout)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "vs_5_0", pDefines, byteCode))
		return false;

	if (FAILED(m_pDevice->CreateInputLayout(pElements, numElements, byteCode.pBytecode, byteCode.bytecodeSize, ppLayout)))
		return false;

	return true;
}

bool ShaderFactory::CreateHullShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11HullShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "hs_5_0", pDefines, byteCode))
		return false;

	if (FAILED(m_pDevice->CreateHullShader(byteCode.pBytecode, byteCode.bytecodeSize, NULL, ppShader)))
		return false;

	return true;
}

bool ShaderFactory::CreateHullShaderEx(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_HULL_SHADER_EX* pArgs, ID3D11HullShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "hs_5_0", pDefines, byteCode))
		return false;

	NvAPI_Status Status = NvAPI_D3D11_CreateHullShaderEx(m_pDevice, byteCode.pBytecode, byteCode.bytecodeSize, NULL, pArgs, ppShader);
	
	if (Status != NVAPI_OK)
		return false;

	return true;
}

bool ShaderFactory::CreateDomainShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11DomainShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "ds_5_0", pDefines, byteCode))
		return false;

	if (FAILED(m_pDevice->CreateDomainShader(byteCode.pBytecode, byteCode.bytecodeSize, NULL, ppShader)))
		return false;

	return true;
}

bool ShaderFactory::CreateDomainShaderEx(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_DOMAIN_SHADER_EX* pArgs, ID3D11DomainShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "ds_5_0", pDefines, byteCode))
		return false;

	NvAPI_Status Status = NvAPI_D3D11_CreateDomainShaderEx(m_pDevice, byteCode.pBytecode, byteCode.bytecodeSize, NULL, pArgs, ppShader);

	if (Status != NVAPI_OK)
		return false;

	return true;
}
bool ShaderFactory::CreateGeometryShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11GeometryShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "gs_5_0", pDefines, byteCode))
		return false;

	if (FAILED(m_pDevice->CreateGeometryShader(byteCode.pBytecode, byteCode.bytecodeSize, NULL, ppShader)))
		return false;

	return true;
}

bool ShaderFactory::CreateFastGeometryShaderExplicit(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_FASTGS_EXPLICIT_DESC* pArgs, ID3D11GeometryShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "gs_5_0", pDefines, byteCode))
		return false;

	NvAPI_Status Status = NvAPI_D3D11_CreateFastGeometryShaderExplicit(m_pDevice, byteCode.pBytecode, byteCode.bytecodeSize, NULL, pArgs, ppShader);
	
	if (Status != NVAPI_OK)
		return false;

	return true;
}

bool ShaderFactory::CreateGeometryShaderEx_2(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_GEOMETRY_SHADER_EX* pArgs, ID3D11GeometryShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "gs_5_0", pDefines, byteCode))
		return false;

	NvAPI_Status Status = NvAPI_D3D11_CreateGeometryShaderEx_2(m_pDevice, byteCode.pBytecode, byteCode.bytecodeSize, NULL, pArgs, ppShader);

	if (Status != NVAPI_OK)
		return false;
	return true;
}

bool ShaderFactory::CreatePixelShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11PixelShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "ps_5_0", pDefines, byteCode))
		return false;

	if (FAILED(m_pDevice->CreatePixelShader(byteCode.pBytecode, byteCode.bytecodeSize, NULL, ppShader)))
		return false;

	return true;
}

bool ShaderFactory::CreateComputeShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11ComputeShader** ppShader)
{
	ShaderByteCode byteCode;
	if (!GetBytecode(fileName, "cs_5_0", pDefines, byteCode))
		return false;

	if (FAILED(m_pDevice->CreateComputeShader(byteCode.pBytecode, byteCode.bytecodeSize, NULL, ppShader)))
		return false;

	return true;
}