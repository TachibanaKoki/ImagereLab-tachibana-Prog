//----------------------------------------------------------------------------------
// File:        vr_sli_demo/shader-common.h
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

#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

// This file is included from both C++ and HLSL; it defines shared resource slot assignments

#ifdef __cplusplus
#	define CBREG(n)						n
#	define TEXREG(n)					n
#	define SAMPREG(n)					n
#else
#	define CBREG(n)						register(b##n)
#	define TEXREG(n)					register(t##n)
#	define SAMPREG(n)					register(s##n)
#endif

#define CB_FRAME						CBREG(0)
#define CB_MATERIAL						CBREG(1)
#define CB_WARP							CBREG(2)
#define CB_VISIBLE_AREA_MASK			CBREG(3)
#define CB_PIECEWISE_GS					CBREG(4)
#define CB_PIECEWISE					CBREG(5)
#define CB_DEBUG						CBREG(6)

#define TEX_DIFFUSE						TEXREG(0)
#define TEX_SPECULAR					TEXREG(1)
#define TEX_BUMP						TEXREG(2)
#define TEX_SHADOW						TEXREG(3)
#define TEX_NORMAL						TEXREG(4)

#define SAMP_DEFAULT					SAMPREG(0)
#define SAMP_SHADOW						SAMPREG(1)

static const int s_msaaSamples = 4;



#ifndef __cplusplus

#pragma pack_matrix(row_major)

struct Vertex
{
	float3		m_pos		: POSITION;
	float3		m_normal	: NORMAL;
	float2		m_uv		: UV;
	float4		m_tangent	: TANGENT;
};

cbuffer CBFrame : CB_FRAME					// matches struct CBFrame in warping_testbed.cpp
{
	float4x4	g_matWorldToClip;
	float4x4	g_matWorldToUvzwShadow;
	float3x3	g_matWorldToUvzShadowNormal;
	float3		g_posCamera;

	float3		g_vecDirectionalLight;
	float3		g_rgbDirectionalLight;

	float3		g_shadowFilterUVZScale;
	float		g_normalOffsetShadow;

	float		g_exposure;					// Exposure multiplier
}

cbuffer CBDebug : CB_DEBUG			// matches struct CBDebug in warping_testbed.cpp
{
	float		g_debugKey;			// Mapped to spacebar - 0 if up, 1 if down
	float		g_debugSlider0;		// Mapped to debug sliders in UI
	float		g_debugSlider1;		// ...
	float		g_debugSlider2;		// ...
	float		g_debugSlider3;		// ...
}

float square(float x) { return x*x; }
float2 square(float2 x) { return x*x; }
float3 square(float3 x) { return x*x; }
float4 square(float4 x) { return x*x; }

#define DEBUG_COLOR 0

#endif // !defined(__cplusplus)
#endif // !defined(SHADER_COMMON_H)
