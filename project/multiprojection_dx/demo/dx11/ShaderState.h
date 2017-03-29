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

#include <framework.h>

using Framework::comptr;
using Framework::RefCount;
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

class ShaderState : public RefCount
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

	Effect								m_Effect;

	// Vertex Shaders
	comptr<ID3D11VertexShader>			m_pVsShadow;
	comptr<ID3D11VertexShader>			m_pVsSafeZone;
	comptr<ID3D11VertexShader>			m_pVsWorld;

	// Geometry Shaders
	comptr<ID3D11GeometryShader>		m_pGsWorld;
	comptr<ID3D11GeometryShader>		m_pGsSafeZone;

	// Pixel Shaders
	comptr<ID3D11PixelShader>			m_pPsAo;
	comptr<ID3D11PixelShader>			m_pPsForward;
	comptr<ID3D11PixelShader>			m_pPsShadowAlphaTest;
	comptr<ID3D11PixelShader>			m_pPsFlatten;
	
	// Compute Shaders
	comptr<ID3D11ComputeShader>			m_pCsTemporalAA;

	// Other stuff
	comptr<ID3D11InputLayout>			m_pInputLayout;

	ShaderState(ShaderFactory* pFactory, Effect effect);
};