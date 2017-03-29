//----------------------------------------------------------------------------------
// File:        vr_sli_demo/shadow.cpp
// SDK Version: 2.1
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

#include "shadow.h"

using namespace util;
using namespace Framework;

// ShadowMap implementation

ShadowMap::ShadowMap()
:	m_vecLight(makefloat3(0.0f)),
	m_boundsScene(makebox3Empty()),
	m_matProj(makefloat4x4(0.0f)),
	m_matWorldToClip(makefloat4x4(0.0f)),
	m_matWorldToUvzw(makefloat4x4(0.0f)),
	m_matWorldToUvzNormal(makefloat3x3(0.0f)),
	m_vecDiam(makefloat3(0.0f))
{
}

void ShadowMap::Init(
	ID3D11Device * pDevice,
	int2_arg dims,
	DXGI_FORMAT format /* = DXGI_FORMAT_D32_FLOAT */)
{
	m_dst.Init(pDevice, dims, format);
	
	LOG("Created shadow map - %dx%d, %s", dims.x, dims.y, NameOfFormat(format));
}

void ShadowMap::Reset()
{
	m_dst.Reset();
	m_vecLight = makefloat3(0.0f);
	m_boundsScene = makebox3Empty();
	m_matProj = makefloat4x4(0.0f);
	m_matWorldToClip = makefloat4x4(0.0f);
	m_matWorldToUvzw = makefloat4x4(0.0f);
	m_matWorldToUvzNormal = makefloat3x3(0.0f);
	m_vecDiam = makefloat3(0.0f);
}

void ShadowMap::UpdateMatrix()
{
	// Calculate view matrix based on light direction

	// Choose an up-vector; handle the light being straight up or down
	float3 vecUp = { 0.0f, 1.0f, 0.0f };
	if (isnear(m_vecLight.x, 0.0f) && isnear(m_vecLight.z, 0.0f))
		vecUp = makefloat3(1.0f, 0.0f, 0.0f);

	affine3 viewToWorld = lookatZ(-m_vecLight, vecUp);
	affine3 worldToView = transpose(viewToWorld);

	// Transform scene AABB into view space and recalculate bounds
	box3 boundsView = boxTransform(m_boundsScene, worldToView);

	m_vecDiam = boundsView.diagonal();

	// Calculate orthogonal projection matrix to fit the scene bounds
	m_matProj = orthoProjD3DStyle(
					boundsView.m_mins.x,
					boundsView.m_maxs.x,
					boundsView.m_mins.y,
					boundsView.m_maxs.y,
					-boundsView.m_maxs.z,
					-boundsView.m_mins.z);

	m_matWorldToClip = affineToHomogeneous(worldToView) * m_matProj;

	// Calculate matrix that maps to [0, 1] UV space instead of [-1, 1] clip space
	float4x4 matClipToUvzw =
	{
		0.5f,  0,    0, 0,
		0,    -0.5f, 0, 0,
		0,     0,    1, 0,
		0.5f,  0.5f, 0, 1,
	};

	m_matWorldToUvzw = m_matWorldToClip * matClipToUvzw;

	// Calculate inverse transpose matrix for transforming normals
	float3x3 matWorldToUvz = makefloat3x3(m_matWorldToUvzw);
	m_matWorldToUvzNormal = transpose(inverse(matWorldToUvz));
}

void ShadowMap::Bind(ID3D11DeviceContext * pCtx)
{
	m_dst.Bind(pCtx);
}

float3 ShadowMap::CalcFilterUVZScale(float filterRadius)
{
	// Expand the filter in the Z direction (this controls how far
	// the filter can tilt before it starts contracting).
	// Tuned empirically.
	float zScale = 4.0f;

	return makefloat3(
			filterRadius / m_vecDiam.x,
			filterRadius / m_vecDiam.y,
			zScale * filterRadius / m_vecDiam.z);
}
