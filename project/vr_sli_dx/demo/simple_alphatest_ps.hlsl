//----------------------------------------------------------------------------------
// File:        vr_sli_demo/simple_alphatest_ps.hlsl
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

#include "shader-common.h"

Texture2D<float4> g_texDiffuse : TEX_DIFFUSE;
Texture2D<float3> g_texDDN : TEX_NORMAL;
SamplerState g_ss : SAMP_DEFAULT;

Texture2D<float> g_texShadowMap : TEX_SHADOW;
SamplerComparisonState g_ssShadow : SAMP_SHADOW;

// Poisson disk generated with http://www.coderhaus.com/?p=11
static const float2 s_aPoisson8[] = 
{ 	
	{ -0.7494944f, 0.1827986f, },
	{ -0.8572887f, -0.4169083f, },
	{ -0.1087135f, -0.05238153f, },
	{ 0.1045462f, 0.9657645f, },
	{ -0.0135659f, -0.698451f, },
	{ -0.4942278f, 0.7898396f, },
	{ 0.7970678f, -0.4682421f, },
	{ 0.8084122f, 0.533884f },
};

float EvaluateShadowPCF8(
	float4 uvzwShadow,
	float3 normalGeom)
{
	float3 uvzShadow = uvzwShadow.xyz / uvzwShadow.w;

	// Calculate surface slope wrt shadow map UV, for tilting the shadow sampling pattern
	float3 normalShadow = mul(normalGeom, g_matWorldToUvzShadowNormal);
	float2 uvSlopes = -normalShadow.xy / normalShadow.z;

	// Apply normal offset to avoid self-shadowing artifacts
	uvzShadow += normalShadow * g_normalOffsetShadow;

	float2 filterScale = g_shadowFilterUVZScale.xy;

	// Reduce filter width if it's tilted too far, to respect a maximum Z offset
	float maxZOffset = length(uvSlopes * filterScale);
	if (maxZOffset > g_shadowFilterUVZScale.z)
	{
		filterScale *= g_shadowFilterUVZScale.z / maxZOffset;
	}

	// Do the samples - each one a bilinear 2x2 PCF tap
	float sampleSum = 0.0;
	[unroll] for (int i = 0; i < 8; ++i)
	{
		float2 uvDelta = s_aPoisson8[i] * filterScale;
		float2 uvSample = uvzShadow.xy + uvDelta;
		float zSample = uvzShadow.z + dot(uvSlopes, uvDelta);
		sampleSum += g_texShadowMap.SampleCmp(g_ssShadow, uvSample, zSample);
	}

	return sampleSum * (1.0 / 8.0);
}

void main(
	in Vertex i_vtx,
	in float3 i_vecCamera : CAMERA,
	in float4 i_uvzwShadow : UVZW_SHADOW,
#if DEBUG_COLOR
	in float4 i_pixelPos : SV_Position,
	in nointerpolation float3 i_rgbDebug : RGB_DEBUG,
#endif
	out float3 o_rgbLight : SV_Target)
{
	float3 bitangent = normalize(cross(i_vtx.m_normal, i_vtx.m_tangent.xyz));
	// Handedness.
	bitangent *= i_vtx.m_tangent.w;

	float3 normalGeom = normalize(i_vtx.m_normal);
	float3 vecCamera = normalize(i_vecCamera);
	float2 uv = i_vtx.m_uv;

	float3 vecNormal_worldSpace = normalGeom;
	
	float3 vecNormal_tangentSpace = g_texDDN.Sample(g_ss, uv).xyz;
	vecNormal_tangentSpace = vecNormal_tangentSpace * 2.0 - 1.0;

	float3x3 matTangentToWorld = float3x3(
		normalize(i_vtx.m_tangent.xyz),
		normalize(bitangent),
		normalGeom
	);

	vecNormal_worldSpace = normalize(mul(vecNormal_tangentSpace, matTangentToWorld));

	float4 diffuseTex = g_texDiffuse.Sample(g_ss, uv);
	if (diffuseTex.a < 0.5)
		discard;

	// Evaluate shadow map
	float shadow = EvaluateShadowPCF8(i_uvzwShadow, vecNormal_worldSpace);

	// Evaluate diffuse lighting
	float3 diffuseColor = diffuseTex.rgb;
	float3 diffuseLight = shadow * g_rgbDirectionalLight * saturate(dot(vecNormal_worldSpace, g_vecDirectionalLight));

	// Simple ramp ambient
	float3 skyColor = { 0.09, 0.11, 0.2 };
	float3 groundColor = { 0.15, 0.15, 0.15 };
	float3 sideColor = { 0.03, 0.02, 0.01 };
	diffuseLight += lerp(groundColor, skyColor, vecNormal_worldSpace.y * 0.5 + 0.5);
	diffuseLight += sideColor * square(saturate(vecNormal_worldSpace.z));

	o_rgbLight = diffuseColor * diffuseLight;

#if DEBUG_COLOR
	if (g_debugKey)
		o_rgbaLight.rgb = i_rgbDebug;
#endif
}
