//----------------------------------------------------------------------------------
// File:        vr_sli_demo/shadow.h
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

#pragma once
#include <framework.h>

// Very simple shadow map class, fits an orthogonal shadow map around a scene bounding box
class ShadowMap
{
public:
					ShadowMap();
	void			Init(
						ID3D11Device * pDevice,
						util::int2_arg dims,
						DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);
	void			Reset();

	void			UpdateMatrix();
	void			Bind(ID3D11DeviceContext * pCtx);

	util::float3	CalcFilterUVZScale(float filterRadius);

	Framework::DepthStencilTarget	m_dst;
	util::float3					m_vecLight;					// Unit vector toward directional light
	util::box3						m_boundsScene;				// AABB of scene in world space
	util::float4x4					m_matProj;					// Projection matrix
	util::float4x4					m_matWorldToClip;			// Matrix for rendering shadow map
	util::float4x4					m_matWorldToUvzw;			// Matrix for sampling shadow map
	util::float3x3					m_matWorldToUvzNormal;		// Matrix for transforming normals to shadow map space
	util::float3					m_vecDiam;					// Diameter in world units along shadow XYZ axes
};
