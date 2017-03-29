//----------------------------------------------------------------------------------
// File:        world_vs.hlsl
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

StructuredBuffer<float4x4> t_InstanceMatrices : register(t0);

VSOutput main(
	in Vertex i_vtx,
	in uint i_instance : SV_InstanceID
#if STEREO_MODE == STEREO_MODE_INSTANCED // do culling+clipping in VS
	,
	out float clipDistance : SV_ClipDistance,
	out float cullDistance : SV_CullDistance
#endif
	)
{
	VSOutput output;

#if (STEREO_MODE == STEREO_MODE_INSTANCED) 
	float4x4 instanceMatrix = (t_InstanceMatrices[i_instance >> 1]);
#else
	float4x4 instanceMatrix = (t_InstanceMatrices[i_instance]);
#endif

	float4 worldPos = mul(instanceMatrix, float4(i_vtx.m_pos, 1.0));
	i_vtx.m_pos = worldPos.xyz / worldPos.w;

	float4 pos = float4(i_vtx.m_pos, 1.0);

	output.vtx = i_vtx;

#if STEREO_MODE == STEREO_MODE_INSTANCED
	uint eyeIndex = i_instance & 1;
	output.eyeIndex = eyeIndex;
	output.posClip = mul(pos, eyeIndex == 0 ? g_matWorldToClip : g_matWorldToClipR);

	// Move geometry to the correct eye

	output.posClip.x *= 0.5;
	float eyeOffsetScale = eyeIndex == 0 ? -0.5 : 0.5;
	output.posClip.x += eyeOffsetScale * output.posClip.w;

	// Calc clip&cull distances

	clipDistance = eyeIndex == 0 ? -1 : 1;
	clipDistance = clipDistance * output.posClip.x;
	cullDistance = clipDistance;
#elif STEREO_MODE == STEREO_MODE_SINGLE_PASS
	output.posClip = mul(pos, g_matWorldToClip);
	output.posClipRight = mul(pos, g_matWorldToClipR);
#else
	output.posClip = mul(pos, g_matWorldToClip);
#endif

	return output;
}

