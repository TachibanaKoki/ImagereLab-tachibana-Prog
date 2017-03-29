//----------------------------------------------------------------------------------
// File:        nv_vr.hlsli
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

#ifndef NV_VR_HLSLI
#define NV_VR_HLSLI

#define NV_VR_PROJECTION_PLANAR			0
#define NV_VR_PROJECTION_MULTI_RES		1
#define NV_VR_PROJECTION_LENS_MATCHED	2

#ifndef NV_VR_PROJECTION 
#define NV_VR_PROJECTION NV_VR_PROJECTION_PLANAR
#endif

#if NV_VR_PROJECTION == NV_VR_PROJECTION_PLANAR
static const uint NV_VR_Width = 1;
static const uint NV_VR_Height = 1;
#elif NV_VR_PROJECTION == NV_VR_PROJECTION_MULTI_RES
static const uint NV_VR_Width = 3;
static const uint NV_VR_Height = 3;
#elif NV_VR_PROJECTION == NV_VR_PROJECTION_LENS_MATCHED
static const uint NV_VR_Width = 2;
static const uint NV_VR_Height = 2;
#else
#error "Unsupported value for NV_VR_PROJECTION"
#endif


// Use this to pass the constants through a float4 array instead of structure 
// (for engines which do not support structures in constant buffers).
// The array can be explicitly or implicitly casted to NV_VR_FastGSCBData.
// Declare like this: float4 NV_VR_FastGSConstants[NV_VR_FASTGS_DATA_SIZE];
#define NV_VR_FASTGS_DATA_SIZE 2

// Use this to pass the constants through a float4 array instead of structure 
// (for engines which do not support structures in constant buffers).
// The array can be explicitly or implicitly casted to NV_VR_RemapCBData.
// Declare like this: float4 NV_VR_RemapConstants[NV_VR_REMAP_DATA_SIZE];
#define NV_VR_REMAP_DATA_SIZE 11

// Constant buffer data to supply the FastGS for culling primitives per-viewport
// (matches corresponding struct in nv_vr.h)
// Note: needs to be 16-byte-aligned when placed in a constant buffer.
struct NV_VR_FastGSCBData
{
	// Split positions in NDC space (Y-up, [-1,1] range)
	float2 NDCSplitsX;
	float2 NDCSplitsY;

	// Warp factors for Lens-Matched Shading
	float WarpLeft;
	float WarpRight;
	float WarpUp;
	float WarpDown;
};

// Take three vertex positions in clip space, and calculate the
// (conservative) mask of viewports the triangle touches
uint NV_VR_CalculateViewportMask(
	NV_VR_FastGSCBData	CBData,
	float4				Position0,
	float4				Position1,
	float4				Position2)
{
	// Cull triangles entirely behind the near plane or beyond the far plane.
	// Works for both regular and reverse projections.
	if (Position0.z < 0 && Position1.z < 0 && Position2.z < 0 || 
		Position0.z > Position0.w && Position1.z > Position1.w && Position2.z > Position2.w)
		return 0;

	// If triangle has any vertices behind the camera, just give up and send it to all viewports.
	// After culling triangles entirely behind the near plane, this shouldn't be many.
	if (Position0.w <= 0.0 || Position1.w <= 0.0 || Position2.w <= 0.0)
		return (1 << (NV_VR_Width * NV_VR_Height)) - 1;

	// Project the vertices onto XY plane - all of them have positive W at this point
	float2 V0 = Position0.xy / Position0.w;
	float2 V1 = Position1.xy / Position1.w;
	float2 V2 = Position2.xy / Position2.w;

	// Calculate AABB on the XY plane
	float2 BottomLeft = min(min(V0, V1), V2);
	float2 TopRight = max(max(V0, V1), V2);

	// Trivial reject
	if (any(BottomLeft > 1.0) || any(TopRight < -1.0))
		return 0;

	// Now calculate a viewport mask based on which rows and columns does the AABB intersect.
	// Let the HLSL compiler do most of the bit manipulations.

#if NV_VR_PROJECTION == NV_VR_PROJECTION_MULTI_RES

	// Replicates a column mask onto 3 rows
	#define MRS_REPL_X(N) ((N) | ((N) << 3) | ((N) << 6))
		
	// Replicates a row mask onto 3 columns
	#define MRS_REPL_Y(N) ((N) | ((N) << 1) | ((N) << 2))
		
	// Calculates a column mask from column flags (left, middle, right)
	#define MRS_MASK_X(L,M,R) MRS_REPL_X(L | (M << 1) | (R << 2))
		
	// Calculates a row mask from row flags (top, middle, bottom)
	#define MRS_MASK_Y(T,M,B) MRS_REPL_Y(T | (M << 3) | (B << 6))

	// Calculate a mask of columns intersected by the AABB
	uint MaskX = 0;

	// Enable all columns to the right of the left edge of AABB
	if (BottomLeft.x < CBData.NDCSplitsX[0])
		MaskX = MRS_MASK_X(1, 1, 1);
	else if (BottomLeft.x < CBData.NDCSplitsX[1])
		MaskX = MRS_MASK_X(0, 1, 1);
	else
		MaskX = MRS_MASK_X(0, 0, 1);

	// Disable all columns to the right of the right edge of AABB
	if (TopRight.x < CBData.NDCSplitsX[0])
		MaskX &= MRS_MASK_X(1, 0, 0);
	else if (TopRight.x < CBData.NDCSplitsX[1])
		MaskX &= MRS_MASK_X(1, 1, 0);

	// Calculate a mask of rows intersected by the AABB
	uint MaskY = 0;

	// Enable all rows above the bottom edge of AABB
	if (BottomLeft.y < CBData.NDCSplitsY[1])
		MaskY = MRS_MASK_Y(1, 1, 1);
	else if (BottomLeft.y < CBData.NDCSplitsY[0])
		MaskY = MRS_MASK_Y(1, 1, 0);
	else
		MaskY = MRS_MASK_Y(1, 0, 0);

	// Disable all rows above the top edge of AABB
	if (TopRight.y < CBData.NDCSplitsY[1])
		MaskY &= MRS_MASK_Y(0, 0, 1);
	else if (TopRight.y < CBData.NDCSplitsY[0])
		MaskY &= MRS_MASK_Y(0, 1, 1);

	// Intersect the column mask with the row mask
	return MaskX & MaskY;

#elif NV_VR_PROJECTION == NV_VR_PROJECTION_LENS_MATCHED

	// Same algorithm as the MRS branch, just for 2 columns and 2 rows instead of 3

	#define LMS_REPL_X(N) ((N) | ((N) << 2))
	#define LMS_REPL_Y(N) ((N) | ((N) << 1))
	#define LMS_MASK_X(L,R) LMS_REPL_X(L | (R << 1))
	#define LMS_MASK_Y(T,B) LMS_REPL_Y(T | (B << 2))

	uint MaskX = 0;

	if (BottomLeft.x < CBData.NDCSplitsX[0])
		MaskX = LMS_MASK_X(1, 1);
	else
		MaskX = LMS_MASK_X(0, 1);

	if (TopRight.x < CBData.NDCSplitsX[0])
		MaskX &= LMS_MASK_X(1, 0);

	uint MaskY = 0;

	if (BottomLeft.y < CBData.NDCSplitsY[1])
		MaskY = LMS_MASK_Y(1, 1);
	else
		MaskY = LMS_MASK_Y(1, 0);

	if (TopRight.y < CBData.NDCSplitsY[1])
		MaskY &= LMS_MASK_Y(0, 1);

	return MaskX & MaskY;

#else
	// Planar has only 1 viewport
	return 1;
#endif
}

// Constant buffer data to supply the UV-remapping helper functions
// (matches corresponding struct in nv_vr.h)
// Note: needs to be 16-byte-aligned when placed in a constant buffer.
struct NV_VR_RemapCBData
{
	float2 ClipToWindowSplitsX;
	float2 ClipToWindowSplitsY;
	float2 ClipToWindowX0;
	float2 ClipToWindowX1;
	float2 ClipToWindowX2;
	float2 ClipToWindowY0;
	float2 ClipToWindowY1;
	float2 ClipToWindowY2;
	float2 ClipToWindowZ;

	float2 WindowToClipSplitsX;
	float2 WindowToClipSplitsY;
	float2 WindowToClipX0;
	float2 WindowToClipX1;
	float2 WindowToClipX2;
	float2 WindowToClipY0;
	float2 WindowToClipY1;
	float2 WindowToClipY2;
	float2 WindowToClipZ;

	float2 BoundingRectOrigin;
	float2 BoundingRectSize;
	float2 BoundingRectSizeInv;
	float2 Padding;
};

NV_VR_RemapCBData NV_VR_GetPlanarRemapCBData(float2 ViewportOrigin, float2 ViewportSize, float2 ViewportSizeInv)
{
	NV_VR_RemapCBData cbData;

	cbData.BoundingRectOrigin = ViewportOrigin;
	cbData.BoundingRectSize = ViewportSize;
	cbData.BoundingRectSizeInv = ViewportSizeInv;

	cbData.ClipToWindowSplitsX = 0;
	cbData.ClipToWindowSplitsY = 0;
	cbData.ClipToWindowX0 = 0;
	cbData.ClipToWindowX1 = 0;
	cbData.ClipToWindowX2 = 0;
	cbData.ClipToWindowY0 = 0;
	cbData.ClipToWindowY1 = 0;
	cbData.ClipToWindowY2 = 0;
	cbData.ClipToWindowZ = float2(1, 0);

	cbData.WindowToClipSplitsX = 0;
	cbData.WindowToClipSplitsY = 0;
	cbData.WindowToClipX0 = 0;
	cbData.WindowToClipX1 = 0;
	cbData.WindowToClipX2 = 0;
	cbData.WindowToClipY0 = 0;
	cbData.WindowToClipY1 = 0;
	cbData.WindowToClipY2 = 0;
	cbData.WindowToClipZ = float2(1, 0);

	cbData.Padding = 0;

	return cbData;
}

#if NV_VR_PROJECTION == NV_VR_PROJECTION_PLANAR

float4 NV_VR_MapWindowToClip(
	NV_VR_RemapCBData	cbData,
	float3				windowPos,
	bool				normalize = true)
{
	float2 UV = (windowPos.xy - cbData.BoundingRectOrigin) * cbData.BoundingRectSizeInv;
	float z = windowPos.z * cbData.WindowToClipZ.x + cbData.WindowToClipZ.y;
	float4 clipPos = float4(UV.x * 2 - 1, -UV.y * 2 + 1, z, 1);
	return clipPos;
}

float3 NV_VR_MapClipToWindow(
	NV_VR_RemapCBData	cbData,
	float4				clipPos,
	bool				normalize = true)
{
	if (normalize)
	{
		clipPos.xyz /= clipPos.w;
		clipPos.w = 1;
	}

	float2 UV = float2(clipPos.x * 0.5 + 0.5, -clipPos.y * 0.5 + 0.5);
	float3 windowPos;
	windowPos.xy = UV * cbData.BoundingRectSize + cbData.BoundingRectOrigin;
	windowPos.z = clipPos.z * cbData.ClipToWindowZ.x + cbData.ClipToWindowZ.y;
	return windowPos;
}

#elif NV_VR_PROJECTION == NV_VR_PROJECTION_LENS_MATCHED


struct NV_VR_LMS_Configuration
{
	float WarpLeft;
	float WarpRight;
	float WarpUp;
	float WarpDown;
};

NV_VR_LMS_Configuration NV_VR_LMS_GetConfiguration(NV_VR_FastGSCBData cbData)
{
	NV_VR_LMS_Configuration res;

	res.WarpLeft	= cbData.WarpLeft;
	res.WarpRight	= cbData.WarpRight;
	res.WarpUp		= cbData.WarpUp;
	res.WarpDown	= cbData.WarpDown;

	return res;
};

NV_VR_LMS_Configuration NV_VR_LMS_GetConfiguration(NV_VR_RemapCBData cbData)
{
	NV_VR_LMS_Configuration res;

	res.WarpLeft	= cbData.ClipToWindowSplitsX.x;
	res.WarpRight	= cbData.ClipToWindowSplitsX.y;
	res.WarpUp		= cbData.ClipToWindowSplitsY.x;
	res.WarpDown	= cbData.ClipToWindowSplitsY.y;

	return res;
};

float2 NV_VR_LMS_GetWarpFactors(float2 clip_pos, NV_VR_LMS_Configuration c)
{
	float2 f;
	f.x = clip_pos.x < 0 ? -c.WarpLeft : +c.WarpRight;
	f.y = clip_pos.y < 0 ? -c.WarpDown : +c.WarpUp;
	return f;
}

float2 NV_VR_LMS_GetWarpFactors(uint viewport, NV_VR_LMS_Configuration c)
{
	float2 f;
	f.x = ((viewport == 0) || (viewport == 2)) ? -c.WarpLeft : +c.WarpRight;
	f.y = ((viewport == 2) || (viewport == 3)) ? -c.WarpDown : +c.WarpUp;
	return f;
}

float4 NV_VR_MapWindowToClip(
	NV_VR_RemapCBData	cbData,
	float3				windowPos,
	bool				normalize = true)
{
	NV_VR_LMS_Configuration conf = NV_VR_LMS_GetConfiguration(cbData);

	float A, B;
	float2 ViewportX, ViewportY;

	if (windowPos.x < cbData.WindowToClipSplitsX[0])
	{
		A = +conf.WarpLeft;
		ViewportX = cbData.WindowToClipX0;
	}
	else
	{
		A = -conf.WarpRight;
		ViewportX = cbData.WindowToClipX1;
	}

	if (windowPos.y < cbData.WindowToClipSplitsY[0])
	{
		B = -conf.WarpUp;
		ViewportY = cbData.WindowToClipY0;
	}
	else
	{
		B = +conf.WarpDown;
		ViewportY = cbData.WindowToClipY1;
	}

	float4 clipPos;
	clipPos.x = windowPos.x * ViewportX.x + ViewportX.y;
	clipPos.y = windowPos.y * ViewportY.x + ViewportY.y;
	clipPos.z = windowPos.z * cbData.WindowToClipZ.x + cbData.WindowToClipZ.y;
	clipPos.w = 1.0 + clipPos.x * A + clipPos.y * B;

	if (normalize)
	{
		clipPos.xyz /= clipPos.w;
		clipPos.w = 1.0;
	}

	return clipPos;
}

float3 NV_VR_MapClipToWindow(
	NV_VR_RemapCBData	cbData,
	float4				clipPos,
	bool				normalize = true)
{
	NV_VR_LMS_Configuration conf = NV_VR_LMS_GetConfiguration(cbData);

	float A, B;
	float2 ViewportX, ViewportY;

	if (clipPos.x < 0)
	{
		A = -conf.WarpLeft;
		ViewportX = cbData.ClipToWindowX0;
	}
	else
	{
		A = +conf.WarpRight;
		ViewportX = cbData.ClipToWindowX1;
	}

	if (clipPos.y < 0)
	{
		B = -conf.WarpDown;
		ViewportY = cbData.ClipToWindowY0;
	}
	else
	{
		B = +conf.WarpUp;
		ViewportY = cbData.ClipToWindowY1;
	}

	float4 warpedPos = clipPos;
	warpedPos.w = clipPos.w + clipPos.x * A + clipPos.y * B;
	warpedPos.xyz /= warpedPos.w;
	warpedPos.w = 1.0;

	float3 windowPos;
	windowPos.x = warpedPos.x * ViewportX.x + ViewportX.y;
	windowPos.y = warpedPos.y * ViewportY.x + ViewportY.y;
	windowPos.z = warpedPos.z * cbData.ClipToWindowZ.x + cbData.ClipToWindowZ.y;

	return windowPos;
}

#elif NV_VR_PROJECTION == NV_VR_PROJECTION_MULTI_RES

float4 NV_VR_MapWindowToClip(
	NV_VR_RemapCBData	cbData,
	float3				windowPos,
	bool				normalize = true)
{
	float4 result;

	if (windowPos.x < cbData.WindowToClipSplitsX.x)
	{
		result.x = windowPos.x * cbData.WindowToClipX0.x + cbData.WindowToClipX0.y;
	}
	else if (windowPos.x < cbData.WindowToClipSplitsX.y)
	{
		result.x = windowPos.x * cbData.WindowToClipX1.x + cbData.WindowToClipX1.y;
	}
	else
	{
		result.x = windowPos.x * cbData.WindowToClipX2.x + cbData.WindowToClipX2.y;
	}

	if (windowPos.y < cbData.WindowToClipSplitsY.x)
	{
		result.y = windowPos.y * cbData.WindowToClipY0.x + cbData.WindowToClipY0.y;
	}
	else if (windowPos.y < cbData.WindowToClipSplitsY.y)
	{
		result.y = windowPos.y * cbData.WindowToClipY1.x + cbData.WindowToClipY1.y;
	}
	else
	{
		result.y = windowPos.y * cbData.WindowToClipY2.x + cbData.WindowToClipY2.y;
	}

	result.z = windowPos.z * cbData.WindowToClipZ.x + cbData.WindowToClipZ.y;
	result.w = 1;

	return result;
}

float3 NV_VR_MapClipToWindow(
	NV_VR_RemapCBData	cbData,
	float4				clipPos,
	bool				normalize = true)
{
	float3 result;

	if (normalize)
	{
		clipPos.xyz /= clipPos.w;
		clipPos.w = 1;
	}

	if (clipPos.x < cbData.ClipToWindowSplitsX.x)
	{
		result.x = clipPos.x * cbData.ClipToWindowX0.x + cbData.ClipToWindowX0.y;
	}
	else if (clipPos.x < cbData.ClipToWindowSplitsX.y)
	{
		result.x = clipPos.x * cbData.ClipToWindowX1.x + cbData.ClipToWindowX1.y;
	}
	else
	{
		result.x = clipPos.x * cbData.ClipToWindowX2.x + cbData.ClipToWindowX2.y;
	}

	if (clipPos.y < cbData.ClipToWindowSplitsY.x)
	{
		result.y = clipPos.y * cbData.ClipToWindowY0.x + cbData.ClipToWindowY0.y;
	}
	else if (clipPos.y < cbData.ClipToWindowSplitsY.y)
	{
		result.y = clipPos.y * cbData.ClipToWindowY1.x + cbData.ClipToWindowY1.y;
	}
	else
	{
		result.y = clipPos.y * cbData.ClipToWindowY2.x + cbData.ClipToWindowY2.y;
	}

	result.z = clipPos.z * cbData.ClipToWindowZ.x + cbData.ClipToWindowZ.y;

	return result;
}

#endif

// Convenience functions that operate on UV instead of clipPos and/or windowPos.
// Linear UV = (0, 0) maps to clip (-1, 1) - top left corner;
// Linear UV = (1, 1) maps to clip (1, -1) - bottom right corner.
// VR "UV" are defined as coordinates relative to BoundingRect.

float2 NV_VR_MapUVToWindow(
	NV_VR_RemapCBData	cbData,
	float2				linearUV)
{
	float4 clipPos = float4(linearUV.x * 2 - 1, -linearUV.y * 2 + 1, 0, 1);
	float3 windowPos = NV_VR_MapClipToWindow(cbData, clipPos, false);
	return windowPos.xy;
}

float2 NV_VR_MapUV_LinearToVR(
	NV_VR_RemapCBData	cbData,
	float2				linearUV)
{
	float4 clipPos = float4(linearUV.x * 2 - 1, -linearUV.y * 2 + 1, 0, 1);
	float3 windowPos = NV_VR_MapClipToWindow(cbData, clipPos, false);
	float2 vrUV = (windowPos.xy - cbData.BoundingRectOrigin) * cbData.BoundingRectSizeInv;
	return vrUV;
}

float2 NV_VR_MapUV_VRToLinear(
	NV_VR_RemapCBData	cbData,
	float2				vrUV)
{
	float2 windowPos = vrUV * cbData.BoundingRectSize + cbData.BoundingRectOrigin;
	float4 clipPos = NV_VR_MapWindowToClip(cbData, float3(windowPos, 0));
	float2 linearUV = float2(clipPos.x * 0.5 + 0.5, -clipPos.y * 0.5 + 0.5);
	return linearUV;
}

#endif // NV_VR_HLSLI
