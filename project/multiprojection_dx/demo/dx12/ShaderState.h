//----------------------------------------------------------------------------------
// File:        ShaderState.h
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
 
#include "GFSDK_NVRHI.h"
#include <util.h>

class ShaderFactory;
enum NV_VR_Projection;

enum struct StereoMode
{
	NONE = 0,
	INSTANCED,
	SINGLE_PASS
};

enum struct NvFeatureLevel
{
	GENERIC_DX11,
	MAXWELL_GPU,
	PASCAL_GPU
};

struct LineVertex	// Matches struct LineVertex in lines_vs.hlsl and lines_ps.hlsl
{
	util::float4	m_rgba;
	util::float4	m_posClip;
};

class ShaderState
{
public:

#pragma warning(push)
#pragma warning(disable : 4201) // nameless struct / union
	union Effect
	{
		struct
		{
			unsigned int projection : 2;
			unsigned int stereoMode : 2;
			unsigned int featureLevel : 2;
			unsigned int msaaSampleCount : 4;
			unsigned int temporalAA : 1;
		};

		unsigned int value;
	};
#pragma warning(pop)

	NVRHI::IRendererInterface*  m_pRenderer;
	Effect						m_Effect;

	// Vertex Shaders
	NVRHI::ShaderHandle			m_pVsShadow;
	NVRHI::ShaderHandle			m_pVsSafeZone;
	NVRHI::ShaderHandle			m_pVsWorld;
	NVRHI::ShaderHandle			m_pVsFullscreen;
	NVRHI::ShaderHandle			m_pVsLines;

	// Geometry Shaders
	NVRHI::ShaderHandle			m_pGsWorld;
	NVRHI::ShaderHandle			m_pGsSafeZone;

	// Pixel Shaders
	NVRHI::ShaderHandle			m_pPsAo;
	NVRHI::ShaderHandle			m_pPsForward;
	NVRHI::ShaderHandle			m_pPsShadowAlphaTest;
	NVRHI::ShaderHandle			m_pPsFlatten;
	NVRHI::ShaderHandle			m_pPsBlit;
	NVRHI::ShaderHandle			m_pPsLines;

	// Compute Shaders
	NVRHI::ShaderHandle			m_pCsTemporalAA;

	// Other stuff
	NVRHI::InputLayoutHandle	m_pInputLayout;
	NVRHI::InputLayoutHandle	m_pInputLayoutLines;

	ShaderState(ShaderFactory* pFactory, NVRHI::IRendererInterface*  pRenderer, Effect effect);
	~ShaderState();
};