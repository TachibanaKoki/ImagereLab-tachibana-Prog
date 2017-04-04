//----------------------------------------------------------------------------------
// File:        ShaderState.cpp
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

#include "ShaderState.h"
#include "ShaderFactory.h"
#include "Scene.h"
#include "nv_vr.h"

ShaderState::ShaderState(ShaderFactory* pFactory, Effect effect)
	: m_Effect(effect)
{
	// Load shaders

	static const char* booleanStrings[] = { "0", "1" };
	static const char* numberStrings[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
	static const char* stereoStrings[] = { "STEREO_MODE_NONE", "STEREO_MODE_INSTANCED", "STEREO_MODE_SINGLE_PASS" };
	static const char* projectionStrings[] = { "NV_VR_PROJECTION_PLANAR", "NV_VR_PROJECTION_MULTI_RES", "NV_VR_PROJECTION_LENS_MATCHED" };

	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "MOTION_VECTORS", booleanStrings[effect.temporalAA] });
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[effect.projection] });
		Macros.push_back(shaderMacro{ "STEREO_MODE", stereoStrings[effect.stereoMode] });

		pFactory->CreatePixelShader("simple_ps", &Macros, &m_pPsForward);
	}

	pFactory->CreatePixelShader("shadow_alphatest_ps", nullptr, &m_pPsShadowAlphaTest);

	if (Nv::VR::Projection(effect.projection) != Nv::VR::Projection::PLANAR)
	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[effect.projection] });
		pFactory->CreatePixelShader("flatten_ps", &Macros, &m_pPsFlatten);
	}

	if (Nv::VR::Projection(effect.projection) != Nv::VR::Projection::PLANAR || StereoMode(effect.stereoMode) == StereoMode::SINGLE_PASS)
	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[effect.projection] });
		Macros.push_back(shaderMacro{ "STEREO_MODE", stereoStrings[effect.stereoMode] });

		bool emulation = false;

		if ((Nv::VR::Projection(effect.projection) == Nv::VR::Projection::LENS_MATCHED || StereoMode(effect.stereoMode) == StereoMode::SINGLE_PASS)
			&& NvFeatureLevel(effect.featureLevel) < NvFeatureLevel::PASCAL_GPU)
			emulation = true;

		if (Nv::VR::Projection(effect.projection) == Nv::VR::Projection::MULTI_RES && NvFeatureLevel(effect.featureLevel) < NvFeatureLevel::MAXWELL_GPU)
			emulation = true;

		Macros.push_back(shaderMacro{ "NV_VR_FASTGS_EMULATION", booleanStrings[emulation] });
		
		if (Nv::VR::Projection(effect.projection) == Nv::VR::Projection::MULTI_RES && !emulation)
		{
			Macros.push_back(shaderMacro{ "NV_VR_FASTGS_VIEWPORT_MASK_COMPATIBILITY", "1" });

			NvAPI_D3D11_CREATE_FASTGS_EXPLICIT_DESC FastGSArgs =
			{
				NVAPI_D3D11_CREATEFASTGSEXPLICIT_VER,
				NV_FASTGS_USE_VIEWPORT_MASK,
			};

			pFactory->CreateFastGeometryShaderExplicit("world_gs", &Macros, &FastGSArgs, &m_pGsWorld);
		}
		else // Lens-Matched Shading or Planar + Single Pass Stereo
		{
			if (emulation)
			{
				pFactory->CreateGeometryShader("world_gs", &Macros, &m_pGsWorld);
				pFactory->CreateGeometryShader("safezone_gs", &Macros, &m_pGsSafeZone);
			}
			else
			{
				NV_CUSTOM_SEMANTIC semantics[] = {
					{ NV_CUSTOM_SEMANTIC_VERSION, NV_X_RIGHT_SEMANTIC, "NV_X_RIGHT", false, 0, 0, 0 },
					{ NV_CUSTOM_SEMANTIC_VERSION, NV_VIEWPORT_MASK_SEMANTIC, "NV_VIEWPORT_MASK", false, 0, 0, 0 }
				};

				NvAPI_D3D11_CREATE_GEOMETRY_SHADER_EX ExDesc = { 0 };
				ExDesc.version = NVAPI_D3D11_CREATEGEOMETRYSHADEREX_2_VERSION;
				ExDesc.ForceFastGS = true;
				ExDesc.NumCustomSemantics = ARRAYSIZE(semantics);
				ExDesc.pCustomSemantics = semantics;

				pFactory->CreateGeometryShaderEx_2("world_gs", &Macros, &ExDesc, &m_pGsWorld);

				Macros[1].Definition = "STEREO_MODE_NONE";
				pFactory->CreateGeometryShaderEx_2("safezone_gs", &Macros, &ExDesc, &m_pGsSafeZone);
			}

			pFactory->CreateVertexShader("safezone_vs", nullptr, &m_pVsSafeZone);
		}
	}

	if(effect.temporalAA)
	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "SAMPLE_COUNT", numberStrings[effect.msaaSampleCount] });
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[int(effect.projection)] });

		pFactory->CreateComputeShader("taa_cs", &Macros, &m_pCsTemporalAA);
	}

	if (effect.computeMSAA)
	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "SAMPLE_COUNT", numberStrings[effect.msaaSampleCount] });
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[int(effect.projection)] });

		pFactory->CreateComputeShader("CompShader", &Macros, &m_pCsMSAA);
	}

	pFactory->CreateVertexShader("world_vs", nullptr, &m_pVsShadow);

	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "STEREO_MODE", stereoStrings[effect.stereoMode] });

		if (StereoMode(effect.stereoMode) == StereoMode::SINGLE_PASS && NvFeatureLevel(effect.featureLevel) == NvFeatureLevel::PASCAL_GPU)
		{
			NV_CUSTOM_SEMANTIC semantics[] = {
				{ NV_CUSTOM_SEMANTIC_VERSION, NV_X_RIGHT_SEMANTIC, "NV_X_RIGHT", false, 0, 0, 0 }
			};

			NvAPI_D3D11_CREATE_VERTEX_SHADER_EX ExDesc = { 0 };
			ExDesc.version = NVAPI_D3D11_CREATEVERTEXSHADEREX_VERSION;
			ExDesc.NumCustomSemantics = ARRAYSIZE(semantics);
			ExDesc.pCustomSemantics = semantics;

			pFactory->CreateVertexShaderEx("world_vs", &Macros, &ExDesc, &m_pVsWorld);
		}
		else
		{
			pFactory->CreateVertexShader("world_vs", &Macros, &m_pVsWorld);
		}
	}

	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[int(effect.projection)] });
		Macros.push_back(shaderMacro{ "SAMPLE_COUNT", numberStrings[effect.msaaSampleCount] });
		pFactory->CreatePixelShader("ao_ps", &Macros, &m_pPsAo);
	}


	// Initialize the input layout, and validate it against all the vertex shaders
	D3D11_INPUT_ELEMENT_DESC aInputDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, UINT(offsetof(Vertex, m_pos)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, UINT(offsetof(Vertex, m_texcoord)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R8G8B8A8_SNORM, 0, UINT(offsetof(Vertex, m_normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R8G8B8A8_SNORM, 0, UINT(offsetof(Vertex, m_tangent)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BITANGENT", 0, DXGI_FORMAT_R8G8B8A8_SNORM, 0, UINT(offsetof(Vertex, m_bitangent)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	pFactory->CreateInputLayout("world_vs", nullptr, aInputDescs, dim(aInputDescs), &m_pInputLayout);
}