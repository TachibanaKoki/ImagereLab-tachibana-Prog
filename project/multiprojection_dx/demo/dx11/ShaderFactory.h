//----------------------------------------------------------------------------------
// File:        ShaderFactory.h
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
#include <framework.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <nvapi.h>

typedef D3D_SHADER_MACRO shaderMacro;

struct ShaderCacheEntry
{
	const char* data;
	size_t sizeOfData;

	ShaderCacheEntry();
	~ShaderCacheEntry();
};

struct ShaderByteCode
{
    const char *pBytecode;
    size_t bytecodeSize;
    UINT hash;
};

class ShaderFactory : public Framework::RefCount
{
private:
    std::unordered_map<UINT64, ShaderCacheEntry> m_BytecodeCache;
	ID3D11Device* m_pDevice;
	std::string m_BasePath;
public:
    ShaderFactory(ID3D11Device* pDevice, const std::string& basePath);

	void ClearCache();
    bool GetBytecode(const char* fileName, const char* target, const std::vector<shaderMacro>* pDefines, ShaderByteCode& byteCode);
	bool CreateVertexShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11VertexShader** ppShader);
	bool CreateVertexShaderEx(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_VERTEX_SHADER_EX* pArgs, ID3D11VertexShader** ppShader);
	bool CreateInputLayout(const char* fileName, const std::vector<shaderMacro>* pDefines, const D3D11_INPUT_ELEMENT_DESC* pElements, UINT numElements, ID3D11InputLayout** ppLayout);
	bool CreateHullShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11HullShader** ppShader);
	bool CreateHullShaderEx(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_HULL_SHADER_EX* pArgs, ID3D11HullShader** ppShader);
	bool CreateDomainShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11DomainShader** ppShader);
	bool CreateDomainShaderEx(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_DOMAIN_SHADER_EX* pArgs, ID3D11DomainShader** ppShader);
	bool CreateGeometryShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11GeometryShader** ppShader);
	bool CreateFastGeometryShaderExplicit(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_FASTGS_EXPLICIT_DESC* pArgs, ID3D11GeometryShader** ppShader);
	bool CreateGeometryShaderEx_2(const char* fileName, const std::vector<shaderMacro>* pDefines, const NvAPI_D3D11_CREATE_GEOMETRY_SHADER_EX* pArgs, ID3D11GeometryShader** ppShader);
	bool CreatePixelShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11PixelShader** ppShader);
	bool CreateComputeShader(const char* fileName, const std::vector<shaderMacro>* pDefines, ID3D11ComputeShader** ppShader);

    static UINT GetNameHash(const std::string& fileName);
    static UINT GetDefinesHash(const std::vector<shaderMacro>* pDefines);
    static UINT Hasher(const std::string& input);
};
