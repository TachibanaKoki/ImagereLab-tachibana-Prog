//----------------------------------------------------------------------------------
// File:        nv_planar.cpp
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
#include <algorithm>

namespace Nv
{
namespace VR
{
	template<>
	void CalculateMirroredConfig<Projection::PLANAR>(const Planar::Configuration& sourceConfig, Planar::Configuration& ref_mirroredConfig)
	{
		ref_mirroredConfig = sourceConfig;
	}

	template<>
	void CalculateViewportsAndBufferData<Projection::PLANAR>(const Float2& flattenedSize, const Viewport& boundingBox, const Planar::Configuration& configuration, Data& ref_data)
	{
		(void)flattenedSize;
		(void)configuration;

		ref_data.Viewports.BoundingRect = ScissorRect{ 
			int(boundingBox.TopLeftX), 
			int(boundingBox.TopLeftY), 
			int(boundingBox.TopLeftX + boundingBox.Width), 
			int(boundingBox.TopLeftY + boundingBox.Height) };
		ref_data.Viewports.Scissors[0] = ref_data.Viewports.BoundingRect;
		ref_data.Viewports.Viewports[0] = boundingBox;
		ref_data.Viewports.NumViewports = 1;
		ref_data.Viewports.FlattenedSize = flattenedSize;

		ref_data.FastGsCbData = {};
		ref_data.RemapCbData = {};
		ref_data.RemapCbData.BoundingRectOrigin = Float2{ float(boundingBox.TopLeftX), float(boundingBox.TopLeftY) };
		ref_data.RemapCbData.BoundingRectSize = Float2{ float(boundingBox.Width), float(boundingBox.Height) };
		ref_data.RemapCbData.BoundingRectSizeInv = Float2{ 1.0f / float(boundingBox.Width), 1.0f / float(boundingBox.Height) };

		ref_data.RemapCbData.ClipToWindowZ.Scale = boundingBox.MaxDepth - boundingBox.MinDepth;
		ref_data.RemapCbData.ClipToWindowZ.Bias = boundingBox.MinDepth;

		ref_data.RemapCbData.WindowToClipZ.Scale = 1.0f / (boundingBox.MaxDepth - boundingBox.MinDepth);
		ref_data.RemapCbData.WindowToClipZ.Bias = -boundingBox.MinDepth * ref_data.RemapCbData.WindowToClipZ.Scale;
	}

	template<>
	Float2 CalculateProjectionSize<Projection::PLANAR>(const Float2& flattenedSize, const Configuration<Projection::PLANAR>& configuration)
	{
		(void)configuration;
		return flattenedSize;
	}

	template<>
	float CalculateRenderedArea<Projection::PLANAR>(const Planar::Configuration& configuration, const ProjectionViewports& viewports)
	{
		(void)configuration;
		return float((viewports.BoundingRect.Right - viewports.BoundingRect.Left) * (viewports.BoundingRect.Bottom - viewports.BoundingRect.Top));
	}

	template<>
	Float3 MapClipToWindow<Projection::PLANAR>(const Data& data, const Float4& clipPos)
	{
		float invW = 1.f / clipPos.w;

		float u = clipPos.x * invW * 0.5f + 0.5f;
		float v = -clipPos.y * invW * 0.5f + 0.5f;

		Float3 windowPos;
		windowPos.x = u * data.RemapCbData.BoundingRectSize.x + data.RemapCbData.BoundingRectOrigin.x;
		windowPos.y = v * data.RemapCbData.BoundingRectSize.y + data.RemapCbData.BoundingRectOrigin.y;
		windowPos.z = clipPos.z * invW * data.RemapCbData.ClipToWindowZ.Scale + data.RemapCbData.ClipToWindowZ.Bias;

		return windowPos;
	}

	template<>
	Float4 MapWindowToClip<Projection::PLANAR>(const Data& data, const Float3& windowPos, bool normalize)
	{
		(void)normalize;

		float u = (windowPos.x - data.RemapCbData.BoundingRectOrigin.x) * data.RemapCbData.BoundingRectSizeInv.x;
		float v = (windowPos.y - data.RemapCbData.BoundingRectOrigin.y) * data.RemapCbData.BoundingRectSizeInv.y;
		float z = windowPos.z * data.RemapCbData.WindowToClipZ.Scale + data.RemapCbData.WindowToClipZ.Bias;

		Float4 clipPos = { u * 2.f - 1.f, -v * 2.f + 1.f, z, 1.f };
		return clipPos;
	}

	void ProjectionViewports::Merge(const ProjectionViewports& in_Left, const ProjectionViewports& in_Right, ProjectionViewports& out)
	{
		//assert(in_Left.NumViewports + in_Right.NumViewports <= MaxCount);

		out.NumViewports = in_Left.NumViewports + in_Right.NumViewports;

		for (int vp = 0; vp < in_Left.NumViewports; vp++)
		{
			out.Viewports[vp] = in_Left.Viewports[vp];
			out.Scissors[vp] = in_Left.Scissors[vp];
		}

		for (int vp = 0; vp < in_Right.NumViewports; vp++)
		{
			out.Viewports[vp + in_Left.NumViewports] = in_Right.Viewports[vp];
			out.Scissors[vp + in_Left.NumViewports] = in_Right.Scissors[vp];
		}

		out.BoundingRect.Left = std::min(in_Left.BoundingRect.Left, in_Right.BoundingRect.Left);
		out.BoundingRect.Top = std::min(in_Left.BoundingRect.Top, in_Right.BoundingRect.Top);
		out.BoundingRect.Right = std::max(in_Left.BoundingRect.Right, in_Right.BoundingRect.Right);
		out.BoundingRect.Bottom = std::max(in_Left.BoundingRect.Bottom, in_Right.BoundingRect.Bottom);
		out.FlattenedSize = in_Left.FlattenedSize; // what else should be there?
	}

	void ProjectionViewports::MakeGrid(
		int Width,
		int Height,
		const int* ScissorBoundsX,
		const int* ScissorBoundsY,
		const ScaleBias* ViewportsX,
		const ScaleBias* ViewportsY,
		float MinDepth,
		float MaxDepth, 
		ProjectionViewports& out)
	{
		//assert(Width * Height <= MaxCount);

		out.NumViewports = Width * Height;

		for (int y = 0; y < Height; y++)
		{
			for (int x = 0; x < Width; x++)
			{
				int i = y * Width + x;

				out.Scissors[i].Left = ScissorBoundsX[x];
				out.Scissors[i].Top = ScissorBoundsY[y];
				out.Scissors[i].Right = ScissorBoundsX[x + 1];
				out.Scissors[i].Bottom = ScissorBoundsY[y + 1];

				out.Viewports[i].TopLeftX = ViewportsX[x].Bias;
				out.Viewports[i].TopLeftY = ViewportsY[y].Bias;
				out.Viewports[i].Width = ViewportsX[x].Scale;
				out.Viewports[i].Height = ViewportsY[y].Scale;
				out.Viewports[i].MinDepth = MinDepth;
				out.Viewports[i].MaxDepth = MaxDepth;
			}
		}

		out.BoundingRect.Left = ScissorBoundsX[0];
		out.BoundingRect.Top = ScissorBoundsY[0];
		out.BoundingRect.Right = ScissorBoundsX[Width];
		out.BoundingRect.Bottom = ScissorBoundsY[Width];
	}
}
}