//----------------------------------------------------------------------------------
// File:        ao_ps.hlsl
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

#include "shader-common.hlsli"

float4 NormalizePoint(float4 pt)
{
	return float4(pt.xyz / pt.w, 1);
}

#if SAMPLE_COUNT==1
Texture2D<float> t_DepthBuffer : register(t0);
Texture2D<float4> t_NormalBuffer : register(t1);
#define LOAD(t, px) t[px]
#else
Texture2DMS<float> t_DepthBuffer : register(t0);
Texture2DMS<float4> t_NormalBuffer : register(t1);
#define LOAD(t, px) t.Load(px, 0)
#endif

static const float2 g_SamplePositions[] = {
	float2(-0.016009523, -0.10995169),
	float2(-0.159746436, 0.047527402),
	float2(0.09339819, 0.201641995),
	float2(0.232600698, 0.151846663),
	float2(-0.220531935, -0.24995355),
	float2(-0.251498143, 0.29661971),
	float2(0.376870668, .23558303),
	float2(0.201175979, 0.457742532),
	float2(-0.535502966, -0.147913991),
	float2(-0.076133435, 0.606350138),
	float2(0.666537538, 0.013120791),
	float2(-0.118107615, -0.712499494),
	float2(-0.740973793, 0.236423582),
	float2(0.365057451, .749117816),
	float2(0.734614792, 0.500464349),
	float2(-0.638657704, -0.695766948)
};

static const float g_SSAO_RadiusWorld = 2.0;
static const float g_SSAO_SurfaceBias = 0.1;
static const float g_SSAO_CoarseAO = 1.0;
static const float g_SSAO_PowerExponent = 2.0;

float2 CalcDelta(float2 pos)
{
	float angle = frac(sin(dot(pos.xy, float2(12.9898,78.233))) * 43758.5453) * 3.1415;
	return float2(sin(angle), cos(angle));
}

float ComputeAOSample(float3 P, float3 N, float3 S, float InvR2)
{
	float3 V = S - P;
	float VdotV = dot(V, V);
	float NdotV = dot(N, V) * rsqrt(VdotV);
	float lambertian = saturate(NdotV - g_SSAO_SurfaceBias);
	float falloff = saturate(1 - VdotV * InvR2);
	return saturate(1.0 - lambertian * falloff * g_SSAO_CoarseAO);
}

float3 ComputeAO(float2 windowPos, float2 delta)
{
	float depth = LOAD(t_DepthBuffer, int2(windowPos.xy)).x;
	float4 clipPos = NV_VR_MapWindowToClip(GetVRRemapCBData(), float3(windowPos, depth));

	if (any(abs(clipPos.xy) > clipPos.w))
		return 1;

	float4 viewPos = NormalizePoint(mul(clipPos, g_matProjInv));

	float3 normal = LOAD(t_NormalBuffer, int2(windowPos.xy)).xyz;
	
	if (all(normal == 0))
		return 1;

	normal = mul(float4(normal, 0), g_matWorldToViewNormal).xyz;


	float invRadiusWorld2 = pow(g_SSAO_RadiusWorld, -2);
	float2 radiusClip = g_SSAO_RadiusWorld * 0.1 / viewPos.z;

	int numSamples = 16;
	float numValidSamples = 0;
	float result = 0;

	for (int nSample = 0; nSample < numSamples; nSample++)
	{
		float2 sampleOffset = g_SamplePositions[nSample];
		sampleOffset = float2(sampleOffset.x * delta.y - sampleOffset.y * delta.x, sampleOffset.x * delta.x + sampleOffset.y * delta.y);

		float4 sampleClipPos = float4(clipPos.xy + sampleOffset * radiusClip, 0, 1);
		float2 sampleWindowPos = NV_VR_MapClipToWindow(GetVRRemapCBData(), sampleClipPos).xy;
		float sampleDepth = LOAD(t_DepthBuffer, int2(sampleWindowPos.xy)).x;
		sampleClipPos = NV_VR_MapWindowToClip(GetVRRemapCBData(), float3(sampleWindowPos, sampleDepth), false);
		float4 sampleViewPos = NormalizePoint(mul(sampleClipPos, g_matProjInv));

		if (sampleViewPos.z > 0)
		{
			float AO = ComputeAOSample(viewPos.xyz, normal, sampleViewPos.xyz, invRadiusWorld2);

			result += AO;
			numValidSamples += 1;
		}
	}

	if (numValidSamples > 0)
		result = pow(saturate(result * rcp(numValidSamples)), g_SSAO_PowerExponent);
	else
		result = 1;

	return result.xxx;
}

void main(
	in float4 pos : SV_Position,
	out float4 o_rgba : SV_Target)
{
	float2 delta = CalcDelta(pos.xy + g_randomOffset);
	float3 ao = ComputeAO(pos.xy, delta);
	o_rgba = float4(ao, 0);
}