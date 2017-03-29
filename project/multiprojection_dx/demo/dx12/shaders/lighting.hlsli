//----------------------------------------------------------------------------------
// File:        lighting.hlsli
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

#ifndef LIGHTING_HLSL
#define LIGHTING_HLSL

// PCF shadow filtering

Texture2D<float> g_texShadowMap : TEX_SHADOW;
SamplerComparisonState g_ssShadow : SAMP_SHADOW;

float EvaluateShadowGather16(
	float4 uvzwShadow,
	float3 normalGeom)
{
	float3 uvzShadow = uvzwShadow.xyz / uvzwShadow.w;

	// Apply normal offset to avoid self-shadowing artifacts
	float3 normalShadow = mul(normalGeom, g_matWorldToUvzShadowNormal);
	uvzShadow += normalShadow * g_normalOffsetShadow;

	// Do the samples - each one a 2x2 GatherCmp
	float4 samplesNW = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2(-1, -1));
	float4 samplesNE = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2( 1, -1));
	float4 samplesSW = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2(-1,  1));
	float4 samplesSE = g_texShadowMap.GatherCmp(g_ssShadow, uvzShadow.xy, uvzShadow.z, int2( 1,  1));

	// Calculate fractional location relative to texel centers.  The 1/512 offset is needed to ensure
	// that frac()'s output steps from 1 to 0 at the exact same point that GatherCmp switches texels.
	float2 offset = frac(uvzShadow.xy * g_dimsShadowMap + (-0.5 + 1.0/512.0));

	// Calculate weights for the samples based on a 2px-radius biquadratic filter
	static const float radius = 2.0;
	float4 xOffsets = offset.x + float4(1, 0, -1, -2);
	float4 yOffsets = offset.y + float4(1, 0, -1, -2);
	// Readable version: xWeights = max(0, 1 - x^2/r^2) for x in xOffsets
	float4 xWeights = saturate(square(xOffsets) * (-1.0 / square(radius)) + 1.0);
	float4 yWeights = saturate(square(yOffsets) * (-1.0 / square(radius)) + 1.0);

	// Calculate weighted sum of samples
	float sampleSum = dot(xWeights.xyyx, yWeights.yyxx * samplesNW) +
					  dot(xWeights.zwwz, yWeights.yyxx * samplesNE) +
					  dot(xWeights.xyyx, yWeights.wwzz * samplesSW) +
					  dot(xWeights.zwwz, yWeights.wwzz * samplesSE);
	float weightSum = dot(xWeights.xyyx, yWeights.yyxx) +
					  dot(xWeights.zwwz, yWeights.yyxx) +
					  dot(xWeights.xyyx, yWeights.wwzz) +
					  dot(xWeights.zwwz, yWeights.wwzz);

	return sharpen(saturate(sampleSum / weightSum), g_shadowSharpening);
}


Texture2D<float4> g_texDiffuse : TEX_DIFFUSE;
Texture2D<float3> g_texNormals : TEX_NORMALS;
Texture2D<float> g_texSpecular : TEX_SPECULAR;
Texture2D<float3> g_texEmissive : TEX_EMISSIVE;
SamplerState g_ss : SAMP_DEFAULT;

float3 GetNormal(Vertex v, float scale)
{
	float3 texNormal = g_texNormals.Sample(g_ss, v.m_uv).xyz;
	float3 N = v.m_normal * scale;

	if (texNormal.z)
	{
		texNormal = texNormal * 2.0 - 1.0;
		N = v.m_tangent * texNormal.x + v.m_bitangent * texNormal.y + v.m_normal * texNormal.z;
	}
	
	return normalize(N);
}

float3 SimpleAmbient(float3 normal)
{
	float3 skyColor = { 0.09, 0.11, 0.2 };
	float3 groundColor = { 0.15, 0.15, 0.15 };
	float3 sideColor = { 0.03, 0.02, 0.01 };
	return lerp(groundColor, skyColor, normal.y * 0.5 + 0.5) + sideColor * square(saturate(normal.z));
}

float4 Lighting(Vertex v, float4 uvShadow, float scale, out float3 o_normal)
{
	float4 albedo = g_texDiffuse.SampleBias(g_ss, v.m_uv, g_textureLodBias);
	float3 emissive = g_texEmissive.SampleBias(g_ss, v.m_uv, g_textureLodBias);

	float3 normal = GetNormal(v, scale);

	float shadow = EvaluateShadowGather16(uvShadow, normal);
	
	float3 ambient = SimpleAmbient(normal) * 0.7;
	float3 diffuse = g_rgbDirectionalLight * saturate(dot(normal, g_vecDirectionalLight));
	float3 reflected = (ambient + diffuse * shadow) * albedo.rgb;

	// Kill the normal for the sky so that SSAO doesn't darken it
	o_normal = any(emissive > 0) ? 0 : normal;

	// Return the alpha value in .w component
	return float4(reflected + emissive, albedo.w);
}

#endif // LIGHTING_HLSL
