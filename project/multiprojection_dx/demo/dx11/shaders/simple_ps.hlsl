//----------------------------------------------------------------------------------
// File:        simple_ps.hlsl
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
#include "lighting.hlsli"

void main(
	in VSOutput input,
#if STEREO_MODE == STEREO_MODE_SINGLE_PASS
	in uint i_viewport : SV_ViewportArrayIndex,
#endif
	in bool i_isFrontFace : SV_IsFrontFace,
	out float4 o_color : SV_Target0,
	out float3 o_normal : SV_Target1
#if MOTION_VECTORS
	, out float2 o_motion : SV_Target2
#endif
)
{
	float4 uvzwShadow = mul(float4(input.vtx.m_pos, 1.0), g_matWorldToUvzwShadow);
	float scale = i_isFrontFace ? 1.0 : -1.0;
	o_color = Lighting(input.vtx, uvzwShadow, scale, o_normal);

#if MOTION_VECTORS
#if STEREO_MODE == STEREO_MODE_INSTANCED
	bool isRightEye = input.eyeIndex != 0;
#elif STEREO_MODE == STEREO_MODE_SINGLE_PASS && NV_VR_PROJECTION == NV_VR_PROJECTION_LENS_MATCHED
	bool isRightEye = i_viewport > 3;
#elif STEREO_MODE == STEREO_MODE_SINGLE_PASS 
	bool isRightEye = i_viewport > 0;
#else
	bool isRightEye = false; // fxc will eliminate the rest of right eye code
#endif

	float4 clipPos, clipPosPrev;
	if (isRightEye)
	{
		clipPos = mul(float4(input.vtx.m_pos, 1.0), g_matWorldToClipR);
		clipPosPrev = mul(float4(input.vtx.m_pos, 1.0), g_matWorldToClipPrevR);
	}
	else
	{
		clipPos = mul(float4(input.vtx.m_pos, 1.0), g_matWorldToClip);
		clipPosPrev = mul(float4(input.vtx.m_pos, 1.0), g_matWorldToClipPrev);
	}

	NV_VR_RemapCBData cbData = GetVRRemapCBData(isRightEye);
	NV_VR_RemapCBData cbDataPrev = GetVRRemapCBDataPrev(isRightEye);
	float2 windowPos = NV_VR_MapClipToWindow(cbData, clipPos).xy;
	float2 windowPosPrev = NV_VR_MapClipToWindow(cbDataPrev, clipPosPrev).xy;

	float2 motion = windowPosPrev.xy - windowPos.xy;
	o_motion = motion;
#endif

}
