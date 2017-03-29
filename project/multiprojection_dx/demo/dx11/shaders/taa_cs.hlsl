//----------------------------------------------------------------------------------
// File:        taa_cs.hlsl
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

#ifndef SAMPLE_COUNT
#define SAMPLE_COUNT 1
#endif

#if SAMPLE_COUNT == 1
Texture2D<float4> t_UnfilteredRT : register(t0);
Texture2D<float2> t_MotionVectors : register(t1);
#else
Texture2DMS<float4> t_UnfilteredRT : register(t0);
Texture2DMS<float2> t_MotionVectors : register(t1);
#endif
Texture2D<float4> t_PrevFilteredRT : register(t2);
SamplerState s_Sampler : register(s0);
RWTexture2D<float4> u_Output : register(u0);

#define GROUP_X 16
#define GROUP_Y 16
#define BUFFER_X (GROUP_X + 2)
#define BUFFER_Y (GROUP_Y + 2)
#define RENAMED_GROUP_Y ((GROUP_X * GROUP_Y) / BUFFER_X)

groupshared float4 s_ColorsAndLengths[BUFFER_Y][BUFFER_X];
groupshared float2 s_MotionVectors[BUFFER_Y][BUFFER_X];
#if NV_VR_PROJECTION != NV_VR_PROJECTION_PLANAR
groupshared int s_CullFlag;
#endif

float3 BicubicSampleCatmullRom(Texture2D tex, SamplerState samp, float2 samplePos, float2 invTextureSize)
{
	float2 tc = floor(samplePos - 0.5) + 0.5;
	float2 f = samplePos - tc;
	float2 f2 = f * f;
	float2 f3 = f2 * f;

	float2 w0 = f2 - 0.5 * (f3 + f);
	float2 w1 = 1.5 * f3 - 2.5 * f2 + 1;
	float2 w3 = 0.5 * (f3 - f2);
	float2 w2 = 1 - w0 - w1 - w3;

	float2 w12 = w1 + w2;
	
	float2 tc0 = (tc - 1) * invTextureSize;
	float2 tc12 = (tc + w2 / w12) * invTextureSize;
	float2 tc3 = (tc + 3) * invTextureSize;

	float3 result =
		tex.SampleLevel(samp, float2(tc0.x,  tc0.y), 0).rgb  * (w0.x  * w0.y) +
		tex.SampleLevel(samp, float2(tc0.x,  tc12.y), 0).rgb * (w0.x  * w12.y) +
		tex.SampleLevel(samp, float2(tc0.x,  tc3.y), 0).rgb  * (w0.x  * w3.y) +
		tex.SampleLevel(samp, float2(tc12.x, tc0.y), 0).rgb  * (w12.x * w0.y) +
		tex.SampleLevel(samp, float2(tc12.x, tc12.y), 0).rgb * (w12.x * w12.y) +
		tex.SampleLevel(samp, float2(tc12.x, tc3.y), 0).rgb  * (w12.x * w3.y) +
		tex.SampleLevel(samp, float2(tc3.x,  tc0.y), 0).rgb  * (w3.x  * w0.y) +
		tex.SampleLevel(samp, float2(tc3.x,  tc12.y), 0).rgb * (w3.x  * w12.y) +
		tex.SampleLevel(samp, float2(tc3.x,  tc3.y), 0).rgb  * (w3.x  * w3.y);

	return result;
}

void Preload(int2 sharedID, int2 globalID)
{
#if SAMPLE_COUNT == 1
	float3 color = t_UnfilteredRT[globalID].rgb;
	float2 motion = t_MotionVectors[globalID].rg;
	float motionLength = dot(motion, motion);
#else
	float3 color = 0;
	float2 motion = 0;
	float motionLength = -1;
	
	// Resolve MSAA color using average filter, motion vectors using max filter

	[unroll]
	for (int nSample = 0; nSample < SAMPLE_COUNT; nSample++)
	{
		float3 sampleColor = t_UnfilteredRT.Load(globalID, nSample).rgb;
		float2 sampleMotion = t_MotionVectors.Load(globalID, nSample).rg;
		float sampleMotionLength = dot(sampleMotion, sampleMotion);

		color += sampleColor;

		if (sampleMotionLength > motionLength)
		{
			motion = sampleMotion;
			motionLength = sampleMotionLength;
		}
	}

	color /= float(SAMPLE_COUNT);
#endif

	s_ColorsAndLengths[sharedID.y][sharedID.x] = float4(color.rgb, motionLength);
	s_MotionVectors[sharedID.y][sharedID.x] = motion;
}

[numthreads(GROUP_X, GROUP_Y, 1)]
void main(
	in int2 i_threadIdx : SV_GroupThreadID,
	in int2 i_globalIdx : SV_DispatchThreadID
)
{
	int2 pixelPosition = i_globalIdx + int2(g_viewportOrigin);

#if NV_VR_PROJECTION != NV_VR_PROJECTION_PLANAR
	
	// Cull the tiles that are outside of the reduced-shading-rate area

	if (all(i_threadIdx.xy == 0))
	{
		s_CullFlag = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	float3 windowPos = float3(float2(pixelPosition) + 0.5, 0);
	
	float4 clipPos = NV_VR_MapWindowToClip(GetVRRemapCBData(), windowPos, false);

	if (all(abs(clipPos.xy) < clipPos.w))
	{
		s_CullFlag = 1;
	}

	GroupMemoryBarrierWithGroupSync();

	if (s_CullFlag == 0)
	{
		// u_Output[pixelPosition] = 0;
		return;
	}
#endif

	// Rename the 16x16 group into a 18x14 group + 4 idle threads in the end

	int2 newID;
	float linearID = i_threadIdx.y * GROUP_X + i_threadIdx.x;
	linearID = (linearID + 0.5) / float(BUFFER_X);
	newID.y = int(floor(linearID));
	newID.x = int(floor(frac(linearID) * BUFFER_X));
	int2 groupBase = pixelPosition - i_threadIdx - 1;
	
	// Preload the colors and motion vectors into shared memory

	if (newID.y < RENAMED_GROUP_Y)
	{
		Preload(newID, groupBase + newID);
	}

	newID.y += RENAMED_GROUP_Y;

	if (newID.y < BUFFER_Y)
	{
		Preload(newID, groupBase + newID);
	}

	GroupMemoryBarrierWithGroupSync();

	// Calculate the color distribution and find the longest MV in the neighbourhood

	float3 colorMoment1 = 0;
	float3 colorMoment2 = 0;
	float longestMVLength = -1;
	int2 longestMVPos = 0;
	float3 thisPixelColor = 0;

	[unroll]
	for (int dy = 0; dy <= 2; dy++)
	{
		[unroll]
		for (int dx = 0; dx <= 2; dx++)
		{
			int2 pos = i_threadIdx.xy + int2(dx, dy);

			float4 colorAndLength = s_ColorsAndLengths[pos.y][pos.x];
			float3 color = colorAndLength.rgb;
			float motionLength = colorAndLength.a;
			
			if (dx == 1 && dy == 1)
			{
				thisPixelColor = color;
			}

			colorMoment1 += color;
			colorMoment2 += color * color;

			if (motionLength > longestMVLength)
			{
				longestMVPos = pos;
				longestMVLength = motionLength;
			}
		}
	}

	float2 longestMV = s_MotionVectors[longestMVPos.y][longestMVPos.x];

	colorMoment1 /= 9.0;
	colorMoment2 /= 9.0;
	float3 colorVariance = colorMoment2 - colorMoment1 * colorMoment1;
	float3 colorSigma = sqrt(max(0, colorVariance)) * g_temporalAAClampingFactor;
	float3 colorMin = colorMoment1 - colorSigma;
	float3 colorMax = colorMoment1 + colorSigma;

	// Sample the previous frame using the longest MV

	float2 sourcePos = float2(pixelPosition.xy) + longestMV + 0.5;
	float3 history = BicubicSampleCatmullRom(t_PrevFilteredRT, s_Sampler, sourcePos, g_screenSize.zw);

	// Clamp the old color to the new color distribution

	float3 historyClamped = history;
	if (g_temporalAAClampingFactor >= 0)
	{
		historyClamped = min(colorMax, max(colorMin, history));
	}

	// Blend the old color with the new color and store output

	float3 result = lerp(historyClamped, thisPixelColor, g_temporalAANewFrameWeight);
	u_Output[pixelPosition] = float4(result, 1.0);
}
