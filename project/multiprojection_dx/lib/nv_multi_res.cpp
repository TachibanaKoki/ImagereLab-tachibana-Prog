//----------------------------------------------------------------------------------
// File:        nv_multi_res.cpp
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
#include <algorithm>	// for std::max/min()

namespace Nv
{
namespace VR
{
	const MultiRes::Configuration MultiRes::Configuration_Balanced =
	{
		0.5f, 0.5f,
		0.5f, 0.5f,
		{ 0.7f, 1.0f, 0.7f },
		{ 0.7f, 1.0f, 0.7f },
	};

	const MultiRes::Configuration MultiRes::Configuration_Aggressive =
	{
		0.4f, 0.4f,
		0.5f, 0.5f,
		{ 0.6f, 1.0f, 0.6f },
		{ 0.6f, 1.0f, 0.6f },
	};

	const MultiRes::Configuration MultiRes::ConfigurationSet_OculusRift_CV1[3] =
	{
		// Conservative
		{ 0.63f, 0.62f, 0.53f, 0.45f,{ 0.62f, 1.0f, 0.75f },{ 0.66f, 1.00f, 0.49f } },
		// Balanced
		{ 0.4f, 0.4f, 0.57f, 0.41f,{ 0.64f, 1.0f, 0.64f },{ 0.64f, 1.0f, 0.64f } },
		// Aggressive
		{ 0.39f, 0.39f, 0.57f, 0.41f,{ 0.43f, 1.0f, 0.43f },{ 0.43f, 1.0f, 0.43f } },
	};

	const MultiRes::Configuration MultiRes::ConfigurationSet_HTC_Vive[3] =
	{
		// Conservative
		{ 0.55f, 0.52f, 0.526f, 0.498f, { 0.61f, 1.142f, 0.61f }, { 0.61f, 1.142f, 0.61f } },
		// Balanced
		{ 0.49f, 0.45f, 0.526f, 0.498f, { 0.6f, 1.0f, 0.6f }, { 0.6f, 1.0f, 0.6f } },
		// Aggressive
		{ 0.47f, 0.42f, 0.526f, 0.498f, { 0.54f, 1.0f, 0.54f }, { 0.54f, 1.0f, 0.54f } },
	};

	template<>
	void CalculateMirroredConfig<Projection::MULTI_RES>(const MultiRes::Configuration& sourceConfig, MultiRes::Configuration& ref_mirroredConfig)
	{
		ref_mirroredConfig.CenterWidth = sourceConfig.CenterWidth;
		ref_mirroredConfig.CenterHeight = sourceConfig.CenterHeight;

		ref_mirroredConfig.CenterX = 1.0f - sourceConfig.CenterX;
		ref_mirroredConfig.CenterY = sourceConfig.CenterY;

		for (int i = 0; i < MultiRes::Configuration::Width; ++i)
		{
			ref_mirroredConfig.DensityScaleX[MultiRes::Configuration::Width - 1 - i] = sourceConfig.DensityScaleX[i];
		}
		for (int i = 0; i < MultiRes::Configuration::Height; ++i)
		{
			ref_mirroredConfig.DensityScaleY[i] = sourceConfig.DensityScaleY[i];
		}
	}

	template<typename T>
	static T Clamp(T value, T min, T max)
	{
		T result = std::max(min, std::min(value, max));
		return result;
	}

	static void CalculateSplits(const Float2& flattenedSize, const MultiRes::Configuration& configuration, float* splitsX, float* splitsY)
	{
		int centerWidth = int(Clamp(configuration.CenterWidth, 0.01f, 1.0f) * flattenedSize.x);
		int centerHeight = int(Clamp(configuration.CenterHeight, 0.01f, 1.0f) * flattenedSize.y);

		int halfCenterWidth = centerWidth / 2;
		int halfCenterHeight = centerHeight / 2;

		int minLeft = halfCenterWidth;
		int maxRight = int(flattenedSize.x - halfCenterWidth);

		int minTop = halfCenterHeight;
		int maxBottom = int(flattenedSize.y - halfCenterHeight);

		float factorX = Clamp(configuration.CenterX, 0.0f, 1.0f);
		int centerLocationX = Clamp(int(factorX * flattenedSize.x), minLeft, maxRight);

		float factorY = Clamp(configuration.CenterY, 0.0f, 1.0f);
		int centerLocationY = Clamp(int(factorY * flattenedSize.y), minTop, maxBottom);

		// Note that first split is min(value,1) and second split is max(value,size-1)
		// It's due to avoid incorrect viewport and scissor errors in the calling method

		int intSplitsX[2];
		intSplitsX[0] = std::max(1, centerLocationX - halfCenterWidth);
		intSplitsX[1] = std::min(int(flattenedSize.x) - 1, intSplitsX[0] + centerWidth);

		int intSplitsY[2];
		intSplitsY[0] = std::max(1, centerLocationY - halfCenterHeight);
		intSplitsY[1] = std::min(int(flattenedSize.y) - 1, intSplitsY[0] + centerHeight);

		float invTotalX = 1.0f / flattenedSize.x;
		float invTotalY = 1.0f / flattenedSize.y;

		splitsX[0] = intSplitsX[0] * invTotalX;
		splitsX[1] = intSplitsX[1] * invTotalX;

		splitsY[0] = intSplitsY[0] * invTotalY;
		splitsY[1] = intSplitsY[1] * invTotalY;
	}

	template<>
	void CalculateViewportsAndBufferData<Projection::MULTI_RES>(const Float2& flattenedSize, const Viewport& boundingBox, const MultiRes::Configuration& configuration, Data& ref_data)
	{
		float splitsX[2];
		float splitsY[2];
		CalculateSplits(flattenedSize, configuration, splitsX, splitsY);

#pragma region Viewports

		{
			int scissorsX[MultiRes::Configuration::Width + 1];
			ScaleBias viewportsX[MultiRes::Configuration::Width];

			scissorsX[0] = int(boundingBox.TopLeftX);
			for (int i = 0; i < MultiRes::Configuration::Width; ++i)
			{
				// Calculate the pixel width of this column of viewports, based on splits and density factor
				float leftSplit = (i == 0) ? 0.0f : splitsX[i - 1];
				float rightSplit = (i == MultiRes::Configuration::Width - 1) ? 1.0f : splitsX[i];
				int scissorWidth = std::max(1, int(round((rightSplit - leftSplit) * configuration.DensityScaleX[i] * flattenedSize.x)));
				scissorsX[i + 1] = scissorsX[i] + scissorWidth;

				// Calculate corresponding viewport position and size
				viewportsX[i].Scale = float(scissorWidth) / (std::max(1e-5f, rightSplit - leftSplit));
				viewportsX[i].Bias = float(scissorsX[i]) - viewportsX[i].Scale * leftSplit;
			}

			int scissorsY[MultiRes::Configuration::Height + 1];
			ScaleBias viewportsY[MultiRes::Configuration::Height];

			scissorsY[0] = int(boundingBox.TopLeftY);
			for (int i = 0; i < MultiRes::Configuration::Height; ++i)
			{
				// Calculate the pixel width of this column of viewports, based on splits and density factor
				float topSplit = (i == 0) ? 0.0f : splitsY[i - 1];
				float bottomSplit = (i == MultiRes::Configuration::Height - 1) ? 1.0f : splitsY[i];
				int scissorHeight = std::max(1, int(round((bottomSplit - topSplit) * configuration.DensityScaleY[i] * flattenedSize.y)));
				scissorsY[i + 1] = scissorsY[i] + scissorHeight;

				// Calculate corresponding viewport position and size
				viewportsY[i].Scale = float(scissorHeight) / (std::max(1e-5f, bottomSplit - topSplit));
				viewportsY[i].Bias = float(scissorsY[i]) - viewportsY[i].Scale * topSplit;
			}

			ProjectionViewports::MakeGrid(MultiRes::Configuration::Width, MultiRes::Configuration::Height, scissorsX, scissorsY, viewportsX, viewportsY, boundingBox.MinDepth, boundingBox.MaxDepth, ref_data.Viewports);
			ref_data.Viewports.FlattenedSize = flattenedSize;
		}

#pragma endregion

#pragma region FastGS

		{
			ref_data.FastGsCbData = { 0 };

			for (int i = 0; i < MultiRes::Configuration::Width - 1; ++i)
			{
				ref_data.FastGsCbData.NDCSplitsX[i] = splitsX[i] * 2.0f - 1.0f;
			}
			for (int i = 0; i < MultiRes::Configuration::Height - 1; ++i)
			{
				ref_data.FastGsCbData.NDCSplitsY[i] = splitsY[i] * -2.0f + 1.0f;
			}
		}

#pragma endregion

#pragma region Remap

		{
			// Calculate the clip-to-window transform based on splits and viewports

			ref_data.RemapCbData.ClipToWindowSplitsX[0] = splitsX[0] * 2.f - 1.f;
			ref_data.RemapCbData.ClipToWindowSplitsX[1] = splitsX[1] * 2.f - 1.f;

			ref_data.RemapCbData.ClipToWindowSplitsY[0] = -splitsY[1] * 2.f + 1.f;
			ref_data.RemapCbData.ClipToWindowSplitsY[1] = -splitsY[0] * 2.f + 1.f;

			for (int i = 0; i < MultiRes::Configuration::Width; ++i)
			{
				float Scale = ref_data.Viewports.Viewports[i].Width * 0.5f;
				float Bias = ref_data.Viewports.Viewports[i].TopLeftX + Scale;
				ref_data.RemapCbData.ClipToWindowX[i] = ScaleBias{ Scale, Bias };
			}

			for (int i = 0; i < MultiRes::Configuration::Height; ++i)
			{
				int j = (MultiRes::Configuration::Height - i - 1) * MultiRes::Configuration::Width;
				float Scale = -ref_data.Viewports.Viewports[j].Height * 0.5f;
				float Bias = ref_data.Viewports.Viewports[j].TopLeftY - Scale;
				ref_data.RemapCbData.ClipToWindowY[i] = ScaleBias{ Scale, Bias };
			}

			ref_data.RemapCbData.ClipToWindowZ.Scale = boundingBox.MaxDepth - boundingBox.MinDepth;
			ref_data.RemapCbData.ClipToWindowZ.Bias = boundingBox.MinDepth;

			// Calculate the window-to-clip transform based on scissors and viewports

			ref_data.RemapCbData.WindowToClipSplitsX[0] = float(ref_data.Viewports.Scissors[1].Left);
			ref_data.RemapCbData.WindowToClipSplitsX[1] = float(ref_data.Viewports.Scissors[2].Left);

			ref_data.RemapCbData.WindowToClipSplitsY[0] = float(ref_data.Viewports.Scissors[3].Top);
			ref_data.RemapCbData.WindowToClipSplitsY[1] = float(ref_data.Viewports.Scissors[6].Top);

			for (int i = 0; i < MultiRes::Configuration::Width; ++i)
			{
				float Scale = 2.0f / ref_data.Viewports.Viewports[i].Width;
				float Bias = -ref_data.Viewports.Viewports[i].TopLeftX * Scale - 1.0f;
				ref_data.RemapCbData.WindowToClipX[i] = ScaleBias{ Scale, Bias };
			}

			for (int i = 0; i < MultiRes::Configuration::Height; ++i)
			{
				int j = i * MultiRes::Configuration::Width;
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
	Float2 CalculateProjectionSize<Projection::MULTI_RES>(const Float2& flattenedSize, const Configuration<Projection::MULTI_RES>& configuration)
	{
		// not the optimal solution in terms of performance, but it's definitely correct
		Data projectionData;
		Viewport boundingBox(0.f, 0.f, flattenedSize.x, flattenedSize.y);
		CalculateViewportsAndBufferData(flattenedSize, boundingBox, configuration, projectionData);
		return Float2(float(projectionData.Viewports.BoundingRect.Right), float(projectionData.Viewports.BoundingRect.Bottom));
	}

	template<>
	float CalculateRenderedArea<Projection::MULTI_RES>(const MultiRes::Configuration& configuration, const ProjectionViewports& viewports)
	{
		(void)configuration;
		return float((viewports.BoundingRect.Right - viewports.BoundingRect.Left) * (viewports.BoundingRect.Bottom - viewports.BoundingRect.Top));
	}

	template<>
	Float3 MapClipToWindow<Projection::MULTI_RES>(const Data& data, const Float4& clipPos)
	{
		float invW = 1.f / clipPos.w;

		ScaleBias viewportX;
		if (clipPos.x < data.RemapCbData.ClipToWindowSplitsX[0])
			viewportX = data.RemapCbData.ClipToWindowX[0];
		else if (clipPos.x < data.RemapCbData.ClipToWindowSplitsX[1])
			viewportX = data.RemapCbData.ClipToWindowX[1];
		else
			viewportX = data.RemapCbData.ClipToWindowX[2];

		ScaleBias viewportY;
		if (clipPos.y < data.RemapCbData.ClipToWindowSplitsY[0])
			viewportY = data.RemapCbData.ClipToWindowY[0];
		else if (clipPos.y < data.RemapCbData.ClipToWindowSplitsY[1])
			viewportY = data.RemapCbData.ClipToWindowY[1];
		else
			viewportY = data.RemapCbData.ClipToWindowY[2];

		Float3 windowPos;
		windowPos.x = clipPos.x * invW * viewportX.Scale + viewportX.Bias;
		windowPos.y = clipPos.y * invW * viewportY.Scale + viewportY.Bias;
		windowPos.z = clipPos.z * invW * data.RemapCbData.ClipToWindowZ.Scale + data.RemapCbData.ClipToWindowZ.Bias;
		return windowPos;
	}

	template<>
	Float4 MapWindowToClip<Projection::MULTI_RES>(const Data& data, const Float3& windowPos, bool normalize)
	{
		(void)normalize;

		ScaleBias viewportX;
		if (windowPos.x < data.RemapCbData.WindowToClipSplitsX[0])
			viewportX = data.RemapCbData.WindowToClipX[0];
		else if (windowPos.x < data.RemapCbData.WindowToClipSplitsX[1])
			viewportX = data.RemapCbData.WindowToClipX[1];
		else
			viewportX = data.RemapCbData.WindowToClipX[2];

		ScaleBias viewportY;
		if (windowPos.y < data.RemapCbData.WindowToClipSplitsY[0])
			viewportY = data.RemapCbData.WindowToClipY[0];
		else if (windowPos.y < data.RemapCbData.WindowToClipSplitsY[1])
			viewportY = data.RemapCbData.WindowToClipY[1];
		else
			viewportY = data.RemapCbData.WindowToClipY[2];

		Float4 clipPos;
		clipPos.x = windowPos.x * viewportX.Scale + viewportX.Bias;
		clipPos.y = windowPos.y * viewportY.Scale + viewportY.Bias;
		clipPos.z = windowPos.z * data.RemapCbData.WindowToClipZ.Scale + data.RemapCbData.WindowToClipZ.Bias;
		clipPos.w = 1.f;
		return clipPos;
	}
}
}