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

// TODO include either d3d11.h or d3d12.h depending on build config
// they're needed so that nvapi.h would detect the API and define all the required structures
#include <d3d11.h>
#include <nvapi.h>

ShaderState::ShaderState(ShaderFactory* pFactory, NVRHI::IRendererInterface*  pRenderer, Effect effect)
	: m_Effect(effect)
	, m_pRenderer(pRenderer)

	, m_pVsShadow(nullptr)
	, m_pVsSafeZone(nullptr)
	, m_pVsWorld(nullptr)
	, m_pVsFullscreen(nullptr)
	, m_pVsLines(nullptr)

	, m_pGsWorld(nullptr)
	, m_pGsSafeZone(nullptr)

	, m_pPsAo(nullptr)
	, m_pPsForward(nullptr)
	, m_pPsShadowAlphaTest(nullptr)
	, m_pPsFlatten(nullptr)
	, m_pPsBlit(nullptr)
	, m_pPsLines(nullptr)

	, m_pCsTemporalAA(nullptr)

	, m_pInputLayout(nullptr)
	, m_pInputLayoutLines(nullptr)
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

		pFactory->CreateShader("simple_ps", &Macros, NVRHI::ShaderType::SHADER_PIXEL, &m_pPsForward);
	}

	pFactory->CreateShader("shadow_alphatest_ps", nullptr, NVRHI::ShaderType::SHADER_PIXEL, &m_pPsShadowAlphaTest);

	if (Nv::VR::Projection(effect.projection) != Nv::VR::Projection::PLANAR)
	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[effect.projection] });
		pFactory->CreateShader("flatten_ps", &Macros, NVRHI::ShaderType::SHADER_PIXEL, &m_pPsFlatten);
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

			NVRHI::ShaderDesc desc(NVRHI::ShaderType::SHADER_GEOMETRY);

			desc.fastGSFlags = NVRHI::FastGeometryShaderFlags::Enum(
				NVRHI::FastGeometryShaderFlags::FORCE_FAST_GS | 
				NVRHI::FastGeometryShaderFlags::COMPATIBILITY_MODE | 
				NVRHI::FastGeometryShaderFlags::USE_VIEWPORT_MASK);
			
			pFactory->CreateShader("world_gs", &Macros, desc, &m_pGsWorld);
		}
		else // Lens-Matched Shading or Planar + Single Pass Stereo
		{
			if (emulation)
			{
				pFactory->CreateShader("world_gs", &Macros, NVRHI::ShaderType::SHADER_GEOMETRY, &m_pGsWorld);
				pFactory->CreateShader("safezone_gs", &Macros, NVRHI::ShaderType::SHADER_GEOMETRY, &m_pGsSafeZone);
			}
			else
			{
				NV_CUSTOM_SEMANTIC semantics[] = {
					{ NV_CUSTOM_SEMANTIC_VERSION, NV_X_RIGHT_SEMANTIC, "NV_X_RIGHT", false, 0, 0, 0 },
					{ NV_CUSTOM_SEMANTIC_VERSION, NV_VIEWPORT_MASK_SEMANTIC, "NV_VIEWPORT_MASK", false, 0, 0, 0 }
				};

				NVRHI::ShaderDesc desc(NVRHI::ShaderType::SHADER_GEOMETRY);

				desc.fastGSFlags = NVRHI::FastGeometryShaderFlags::Enum(
					NVRHI::FastGeometryShaderFlags::FORCE_FAST_GS | 
					NVRHI::FastGeometryShaderFlags::USE_VIEWPORT_MASK);

				desc.numCustomSemantics = ARRAYSIZE(semantics);
				desc.pCustomSemantics = semantics;

				pFactory->CreateShader("world_gs", &Macros, desc, &m_pGsWorld);

				Macros[1].Definition = "STEREO_MODE_NONE";
				pFactory->CreateShader("safezone_gs", &Macros, desc, &m_pGsSafeZone);
			}

			pFactory->CreateShader("safezone_vs", nullptr, NVRHI::ShaderType::SHADER_VERTEX, &m_pVsSafeZone);
		}
	}

	if(effect.temporalAA)
	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "SAMPLE_COUNT", numberStrings[effect.msaaSampleCount] });
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[int(effect.projection)] });

		pFactory->CreateShader("taa_cs", &Macros, NVRHI::ShaderType::SHADER_COMPUTE, &m_pCsTemporalAA);
	}


	pFactory->CreateShader("world_vs", nullptr, NVRHI::ShaderType::SHADER_VERTEX, &m_pVsShadow);

	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "STEREO_MODE", stereoStrings[effect.stereoMode] });

		if (StereoMode(effect.stereoMode) == StereoMode::SINGLE_PASS && NvFeatureLevel(effect.featureLevel) == NvFeatureLevel::PASCAL_GPU)
		{
			NV_CUSTOM_SEMANTIC semantics[] = {
				{ NV_CUSTOM_SEMANTIC_VERSION, NV_X_RIGHT_SEMANTIC, "NV_X_RIGHT", false, 0, 0, 0 }
			};

			NVRHI::ShaderDesc desc(NVRHI::ShaderType::SHADER_VERTEX);

			desc.numCustomSemantics = ARRAYSIZE(semantics);
			desc.pCustomSemantics = semantics;

			pFactory->CreateShader("world_vs", &Macros, desc, &m_pVsWorld);
		}
		else
		{
			pFactory->CreateShader("world_vs", &Macros, NVRHI::ShaderType::SHADER_VERTEX, &m_pVsWorld);
		}
	}

	{
		std::vector<shaderMacro> Macros;
		Macros.push_back(shaderMacro{ "NV_VR_PROJECTION", projectionStrings[int(effect.projection)] });
		Macros.push_back(shaderMacro{ "SAMPLE_COUNT", numberStrings[effect.msaaSampleCount] });
		pFactory->CreateShader("ao_ps", &Macros, NVRHI::ShaderType::SHADER_PIXEL, &m_pPsAo);
	}


	// Initialize the input layout, and validate it against all the vertex shaders
	NVRHI::VertexAttributeDesc aInputDescs[] =
	{
		{ "POSITION" , NVRHI::Format::RGB32_FLOAT, 0, UINT(offsetof(Vertex, m_pos))      , false },
		{ "UV"       , NVRHI::Format::RG32_FLOAT , 0, UINT(offsetof(Vertex, m_texcoord)) , false },
		{ "NORMAL"   , NVRHI::Format::RGBA8_SNORM, 0, UINT(offsetof(Vertex, m_normal))   , false },
		{ "TANGENT"  , NVRHI::Format::RGBA8_SNORM, 0, UINT(offsetof(Vertex, m_tangent))  , false },
		{ "BITANGENT", NVRHI::Format::RGBA8_SNORM, 0, UINT(offsetof(Vertex, m_bitangent)), false },
	};
	pFactory->CreateInputLayout("world_vs", nullptr, aInputDescs, dim(aInputDescs), &m_pInputLayout);

	pFactory->CreateShader("fullscreen_vs", nullptr, NVRHI::ShaderType::SHADER_VERTEX, &m_pVsFullscreen);
	pFactory->CreateShader("lines_vs", nullptr, NVRHI::ShaderType::SHADER_VERTEX, &m_pVsLines);
	pFactory->CreateShader("blit_ps", nullptr, NVRHI::ShaderType::SHADER_PIXEL, &m_pPsBlit);
	pFactory->CreateShader("lines_ps", nullptr, NVRHI::ShaderType::SHADER_PIXEL, &m_pPsLines);

	const NVRHI::VertexAttributeDesc linesVertexInputLayout[] =
	{
		{ "COLOR"      , NVRHI::Format::RGBA32_FLOAT, 0, offsetof(LineVertex, m_rgba)   , false },
		{ "SV_Position", NVRHI::Format::RGBA32_FLOAT, 0, offsetof(LineVertex, m_posClip), false },
	};

	pFactory->CreateInputLayout("lines_vs", nullptr, linesVertexInputLayout, dim(linesVertexInputLayout), &m_pInputLayoutLines);
}

ShaderState::~ShaderState()
{
	m_pRenderer->destroyShader(m_pVsShadow);
	m_pRenderer->destroyShader(m_pVsSafeZone);
	m_pRenderer->destroyShader(m_pVsWorld);
	m_pRenderer->destroyShader(m_pVsFullscreen);
	m_pRenderer->destroyShader(m_pVsLines);

	m_pRenderer->destroyShader(m_pGsWorld);
	m_pRenderer->destroyShader(m_pGsSafeZone);

	m_pRenderer->destroyShader(m_pPsAo);
	m_pRenderer->destroyShader(m_pPsForward);
	m_pRenderer->destroyShader(m_pPsShadowAlphaTest);
	m_pRenderer->destroyShader(m_pPsFlatten);
	m_pRenderer->destroyShader(m_pPsBlit);
	m_pRenderer->destroyShader(m_pPsLines)
		;
	m_pRenderer->destroyShader(m_pCsTemporalAA);
	m_pRenderer->destroyShader(m_pCsMSAA);

	m_pRenderer->destroyInputLayout(m_pInputLayout);
	m_pRenderer->destroyInputLayout(m_pInputLayoutLines);
}
