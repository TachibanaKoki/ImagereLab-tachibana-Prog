//----------------------------------------------------------------------------------
// File:        nv_lens_matched_shading.cpp
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

#include "nv_vr.h"
#include <math.h>
#include <algorithm>

namespace Nv
{
namespace VR
{
	// Sizes are relative to Oculus recommended render target size for one eye, 1332 x 1586
	const LensMatched::Configuration LensMatched::ConfigurationSet_OculusRift_CV1[3] =
	{
		// Conservative
		{ 0.47f, 0.47f, 0.47f, 0.47f, 0.414f, 0.553f, 0.534f, 0.368f },
		// Balanced
		{ 0.90f, 0.90f, 0.90f, 0.90f, 0.361f, 0.481f, 0.465f, 0.321f },
		// Aggressive
		{ 0.90f, 0.90f, 0.90f, 0.90f, 0.311f, 0.412f, 0.401f, 0.276f },
	};

	// Sizes are relative to HTC recommended render target size for one eye, 1512 x 1680
	const LensMatched::Configuration LensMatched::ConfigurationSet_HTC_Vive[3] =
	{
		// Conservative
		{ 0.61f, 0.41f, 0.59f, 0.62f, 0.444f, 0.505f, 0.442f, 0.438f },
		// Balanced
		{ 0.90f, 0.67f, 0.88f, 0.91f, 0.397f, 0.451f, 0.395f, 0.391f },
		// Aggressive
		{ 1.00f, 0.76f, 0.98f, 1.01f, 0.347f, 0.394f, 0.345f, 0.341f },
	};

	template<>
	void CalculateMirroredConfig<Projection::LENS_MATCHED>(const LensMatched::Configuration& sourceConfig, LensMatched::Configuration& ref_mirroredConfig)
	{
		ref_mirroredConfig = sourceConfig;
		ref_mirroredConfig.WarpLeft = sourceConfig.WarpRight;
		ref_mirroredConfig.WarpRight = sourceConfig.WarpLeft;
		ref_mirroredConfig.RelativeSizeLeft = sourceConfig.RelativeSizeRight;
		ref_mirroredConfig.RelativeSizeRight = sourceConfig.RelativeSizeLeft;
	}

	template<>
	void CalculateViewportsAndBufferData<Projection::LENS_MATCHED>(const Float2& flattenedSize, const Viewport& boundingBox, const LensMatched::Configuration& configuration, Data& ref_data)
	{
#pragma region Viewports

	{
		float SizeLeft = ceil(configuration.RelativeSizeLeft * flattenedSize.x);
		float SizeRight = ceil(configuration.RelativeSizeRight * flattenedSize.x);
		float SizeUp = ceil(configuration.RelativeSizeUp * flattenedSize.y);
		float SizeDown = ceil(configuration.RelativeSizeDown * flattenedSize.y);

		Float2 center;
		center.x = ceil(boundingBox.TopLeftX) + SizeLeft;
		center.y = ceil(boundingBox.TopLeftY) + SizeUp;

		float viewportLeft = SizeLeft * (1.0f + configuration.WarpLeft);
		float viewportRight = SizeRight * (1.0f + configuration.WarpRight);
		float viewportUp = SizeUp * (1.0f + configuration.WarpUp);
		float viewportDown = SizeDown * (1.0f + configuration.WarpDown);

		ScaleBias viewportsX[] = {
			{ viewportLeft * 2, center.x - viewportLeft },
			{ viewportRight * 2, center.x - viewportRight }
		};

		ScaleBias viewportsY[] = {
			{ viewportUp * 2, center.y - viewportUp },
			{ viewportDown * 2, center.y - viewportDown }
		};

		int scissorsX[] = {
			std::max(int(boundingBox.TopLeftX), int(center.x - SizeLeft)),
			int(center.x),
			std::min(int(boundingBox.TopLeftX + boundingBox.Width), int(center.x + SizeRight))
		};

		int scissorsY[] = {
			std::max(int(boundingBox.TopLeftY), int(center.y - SizeUp)),
			int(center.y),
			std::min(int(boundingBox.TopLeftY + boundingBox.Height), int(center.y + SizeDown))
		};

		ProjectionViewports::MakeGrid(2, 2, scissorsX, scissorsY, viewportsX, viewportsY, boundingBox.MinDepth, boundingBox.MaxDepth, ref_data.Viewports);
		ref_data.Viewports.FlattenedSize = flattenedSize;
	}

#pragma endregion

#pragma region FastGS

	{
		ref_data.FastGsCbData = { 0 };
		ref_data.FastGsCbData.WarpLeft = configuration.WarpLeft;
		ref_data.FastGsCbData.WarpRight = configuration.WarpRight;
		ref_data.FastGsCbData.WarpUp = configuration.WarpUp;
		ref_data.FastGsCbData.WarpDown = configuration.WarpDown;
	}

#pragma endregion

#pragma region Remap

	{
		// Clip to window

		ref_data.RemapCbData.ClipToWindowSplitsX[0] = configuration.WarpLeft;
		ref_data.RemapCbData.ClipToWindowSplitsX[1] = configuration.WarpRight;
		ref_data.RemapCbData.ClipToWindowSplitsY[0] = configuration.WarpUp;
		ref_data.RemapCbData.ClipToWindowSplitsY[1] = configuration.WarpDown;

		for (int i = 0; i < 2; ++i)
		{
			float Scale = ref_data.Viewports.Viewports[i].Width * 0.5f;
			float Bias = ref_data.Viewports.Viewports[i].TopLeftX + Scale;
			ref_data.RemapCbData.ClipToWindowX[i] = ScaleBias{ Scale, Bias };
		}

		for (int i = 0; i < 2; ++i)
		{
			int j = 1 - i;
			float Scale = -ref_data.Viewports.Viewports[j * 2].Height * 0.5f;
			float Bias = ref_data.Viewports.Viewports[j * 2].TopLeftY - Scale;
			ref_data.RemapCbData.ClipToWindowY[i] = ScaleBias{ Scale, Bias };
		}

		ref_data.RemapCbData.ClipToWindowZ.Scale = boundingBox.MaxDepth - boundingBox.MinDepth;
		ref_data.RemapCbData.ClipToWindowZ.Bias = boundingBox.MinDepth;

		// Window to clip

		ref_data.RemapCbData.WindowToClipSplitsX[0] = float(ref_data.Viewports.Scissors[1].Left);
		ref_data.RemapCbData.WindowToClipSplitsX[1] = 0.f;
		ref_data.RemapCbData.WindowToClipSplitsY[0] = float(ref_data.Viewports.Scissors[2].Top);
		ref_data.RemapCbData.WindowToClipSplitsY[1] = 0.f;

		for (int i = 0; i < 2; ++i)
		{
			float Scale = 2.0f / ref_data.Viewports.Viewports[i].Width;
			float Bias = -ref_data.Viewports.Viewports[i].TopLeftX * Scale - 1.0f;
			ref_data.RemapCbData.WindowToClipX[i] = ScaleBias{ Scale, Bias };
		}

		for (int i = 0; i < 2; ++i)
		{
			int j = i * 2;
			float Scale = -2.0f / ref_data.Viewports.Viewports[j].Height;
			float Bias = -ref_data.Viewports.Viewports[j].TopLeftY * Scale + 1.0f;
			ref_data.RemapCbData.WindowToClipY[i] = ScaleBias{ Scale, Bias };
		}

		ref_data.RemapCbData.WindowToClipZ.Scale = 1.0f / (boundingBox.MaxDepth - boundingBox.MinDepth);
		ref_data.RemapCbData.WindowToClipZ.Bias = -boundingBox.MinDepth * ref_data.RemapCbData.WindowToClipZ.Scale;

		// Bounding rect

		ref_data.RemapCbData.BoundingRectOrigin.x = float(ref_data.Viewports.BoundingRect.Left);
		ref_data.RemapCbData.BoundingRectOrigin.y = float(ref_data.Viewports.BoundingRect.Top);
		ref_data.RemapCbData.BoundingRectSize.x = float(ref_data.Viewports.BoundingRect.Right - ref_data.Viewports.BoundingRect.Left);
		ref_data.RemapCbData.BoundingRectSize.y = float(ref_data.Viewports.BoundingRect.Bottom - ref_data.Viewports.BoundingRect.Top);
		ref_data.RemapCbData.BoundingRectSizeInv.x = 1.0f / ref_data.RemapCbData.BoundingRectSize.x;
		ref_data.RemapCbData.BoundingRectSizeInv.y = 1.0f / ref_data.RemapCbData.BoundingRectSize.y;

		ref_data.RemapCbData.Padding[0] = 0.f;
		ref_data.RemapCbData.Padding[1] = 0.f;
	}

#pragma endregion
	}

	template<>
	Float2 CalculateProjectionSize<Projection::LENS_MATCHED>(const Float2& flattenedSize, const Configuration<Projection::LENS_MATCHED>& configuration)
	{
		float SizeLeft = ceil(configuration.RelativeSizeLeft * flattenedSize.x);
		float SizeRight = ceil(configuration.RelativeSizeRight * flattenedSize.x);
		float SizeUp = ceil(configuration.RelativeSizeUp * flattenedSize.y);
		float SizeDown = ceil(configuration.RelativeSizeDown * flattenedSize.y);
		
		return Float2(SizeLeft + SizeRight, SizeUp + SizeDown);
	}

	template<>
	float CalculateRenderedArea<Projection::LENS_MATCHED>(const LensMatched::Configuration&	configuration, const ProjectionViewports& viewports)
	{
		(void)viewports;
		
		auto getQuadrantSize = [](float sizeX, float sizeY, float warpX, float warpY)
		{
			float viewportX = sizeX * (1.0f + warpX);
			float viewportY = sizeY * (1.0f + warpY);
			float midpointX = viewportX / (1.0f + warpX + warpY);
			float midpointY = viewportY / (1.0f + warpX + warpY);

			float rectangleArea = midpointX * midpointY;
			float triangleAreaX = (sizeX - midpointX) * midpointY * 0.5f;
			float triangleAreaY = (sizeY - midpointY) * midpointX * 0.5f;

			return rectangleArea + triangleAreaX + triangleAreaY;
		};

		float SizeLeft = ceil(configuration.RelativeSizeLeft * viewports.FlattenedSize.x);
		float SizeRight = ceil(configuration.RelativeSizeRight * viewports.FlattenedSize.x);
		float SizeUp = ceil(configuration.RelativeSizeUp * viewports.FlattenedSize.x);
		float SizeDown = ceil(configuration.RelativeSizeDown * viewports.FlattenedSize.x);

		float areaUpLeft = getQuadrantSize(SizeLeft, SizeUp, configuration.WarpLeft, configuration.WarpUp);
		float areaUpRight = getQuadrantSize(SizeRight, SizeUp, configuration.WarpRight, configuration.WarpUp);
		float areaDownLeft = getQuadrantSize(SizeLeft, SizeDown, configuration.WarpLeft, configuration.WarpDown);
		float areaDownRight = getQuadrantSize(SizeRight, SizeDown, configuration.WarpRight, configuration.WarpDown);

		return areaUpLeft + areaUpRight + areaDownLeft + areaDownRight;
	}

	template<>
	Float3 MapClipToWindow<Projection::LENS_MATCHED>(const Data& data, const Float4& clipPos)
	{
		float A, B;
		ScaleBias ViewportX, ViewportY;

		if (clipPos.x < 0)
		{
			A = -data.FastGsCbData.WarpLeft;
			ViewportX = data.RemapCbData.ClipToWindowX[0];
		}
		else
		{
			A = +data.FastGsCbData.WarpRight;
			ViewportX = data.RemapCbData.ClipToWindowX[1];
		}

		if (clipPos.y < 0)
		{
			B = -data.FastGsCbData.WarpDown;
			ViewportY = data.RemapCbData.ClipToWindowY[0];
		}
		else
		{
			B = +data.FastGsCbData.WarpUp;
			ViewportY = data.RemapCbData.ClipToWindowY[1];
		}

		float invW = 1.f / (clipPos.w + clipPos.x * A + clipPos.y * B);
		
		Float3 windowPos;
		windowPos.x = clipPos.x * invW * ViewportX.Scale + ViewportX.Bias;
		windowPos.y = clipPos.y * invW * ViewportY.Scale + ViewportY.Bias;
		windowPos.z = clipPos.z * invW * data.RemapCbData.ClipToWindowZ.Scale + data.RemapCbData.ClipToWindowZ.Bias;

		return windowPos;
	}

	template<>
	Float4 MapWindowToClip<Projection::LENS_MATCHED>(const Data& data, const Float3& windowPos, bool normalize)
	{
		float A, B;
		ScaleBias ViewportX, ViewportY;

		if (windowPos.x < data.RemapCbData.WindowToClipSplitsX[0])
		{
			A = +data.FastGsCbData.WarpLeft;
			ViewportX = data.RemapCbData.WindowToClipX[0];
		}
		else
		{
			A = -data.FastGsCbData.WarpRight;
			ViewportX = data.RemapCbData.WindowToClipX[1];
		}

		if (windowPos.y < data.RemapCbData.WindowToClipSplitsY[0])
		{
			B = -data.FastGsCbData.WarpUp;
			ViewportY = data.RemapCbData.WindowToClipY[0];
		}
		else
		{
			B = +data.FastGsCbData.WarpDown;
			ViewportY = data.RemapCbData.WindowToClipY[1];
		}

		Float4 clipPos;
		clipPos.x = windowPos.x * ViewportX.Scale + ViewportX.Bias;
		clipPos.y = windowPos.y * ViewportY.Scale + ViewportY.Bias;
		clipPos.z = windowPos.z * data.RemapCbData.WindowToClipZ.Scale + data.RemapCbData.WindowToClipZ.Bias;
		clipPos.w = 1.f + clipPos.x * A + clipPos.y * B;

		if (normalize)
		{
			float invW = 1.f / clipPos.w;
			clipPos.x *= invW;
			clipPos.y *= invW;
			clipPos.z *= invW;
			clipPos.w = 1.0;
		}

		return clipPos;
	}
}
}