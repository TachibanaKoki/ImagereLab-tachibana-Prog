//----------------------------------------------------------------------------------
// File:        shader-common.hlsli
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

#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

#include "../../../lib/nv_vr.hlsli"
#include "shader-slots.h"

#pragma pack_matrix(row_major)

#ifndef _CENTROID
#define _CENTROID
#endif

struct Vertex
{
	          float3 m_pos			: POSITION;
	          float2 m_uv			: UV;
	centroid  float3 m_normal		: NORMAL;
	centroid  float3 m_tangent		: TANGENT;
	centroid  float3 m_bitangent	: BITANGENT;
};

cbuffer CBFrame : CB_FRAME	// matches struct CBFrame in demo_dx11.cpp
{
	float4x4				g_matWorldToClip;
	float4x4				g_matWorldToClipR;
	float4x4				g_matWorldToClipPrev;
	float4x4				g_matWorldToClipPrevR;
	float4x4				g_matProjInv;
	float4x4				g_matWorldToViewNormal;
	float4x4				g_matWorldToUvzwShadow;
	float3x3				g_matWorldToUvzShadowNormal;
	
	float3					g_vecDirectionalLight;
	float3					g_rgbDirectionalLight;

	float2					g_dimsShadowMap;
	float					g_normalOffsetShadow;
	float					g_shadowSharpening;

	float4					g_screenSize;

	float2					g_viewportOrigin;
	float2					g_randomOffset;
	
	float					g_temporalAAClampingFactor;
	float					g_temporalAANewFrameWeight;
	float					g_textureLodBias;
}

cbuffer CBVr : CB_VR
{
	NV_VR_FastGSCBData		g_vrFastGSCBData;
	NV_VR_FastGSCBData		g_vrFastGSCBDataR;
	NV_VR_RemapCBData		g_vrRemapCBData;
	NV_VR_RemapCBData		g_vrRemapCBDataR;
	NV_VR_RemapCBData		g_vrRemapCBDataPrev;
	NV_VR_RemapCBData		g_vrRemapCBDataPrevR;
};

NV_VR_RemapCBData GetVRRemapCBData(bool isRightEye = false)
{
	if (isRightEye)
		return g_vrRemapCBDataR;
	else
		return g_vrRemapCBData;
}

NV_VR_RemapCBData GetVRRemapCBDataPrev(bool isRightEye = false)
{
	if (isRightEye)
		return g_vrRemapCBDataPrevR;
	else
		return g_vrRemapCBDataPrev;
}

float  square(float  x) { return x*x; }
float2 square(float2 x) { return x*x; }
float3 square(float3 x) { return x*x; }
float4 square(float4 x) { return x*x; }

float sharpen(float x, float sharpening)
{
	if (x < 0.5)
		return 0.5 * pow(2.0*x, sharpening);
	else
		return -0.5 * pow(-2.0*x + 2.0, sharpening) + 1.0;
}

#define STEREO_MODE_NONE 0
#define STEREO_MODE_INSTANCED 1
#define STEREO_MODE_SINGLE_PASS 2

#ifndef STEREO_MODE
#define STEREO_MODE STEREO_MODE_NONE
#endif

struct VSOutput
{
	Vertex vtx;
#if STEREO_MODE == STEREO_MODE_INSTANCED
	uint eyeIndex : EYE;
#elif STEREO_MODE == STEREO_MODE_SINGLE_PASS
	float4 posClipRight : NV_X_RIGHT;
#endif
	float4 posClip : SV_Position;
};

struct SafeZoneVSOutput
{
	float4 posClip : SV_Position;
	float4 posClipRight : NV_X_RIGHT;
};

#endif // SHADER_COMMON_HLSLI
