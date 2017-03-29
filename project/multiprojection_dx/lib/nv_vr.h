//----------------------------------------------------------------------------------
// File:        nv_vr.h
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

#pragma once

namespace Nv
{
namespace VR
{
	enum class Projection
	{
		PLANAR,
		MULTI_RES,
		LENS_MATCHED
	};


	// ============================================================================
	// Common types for all projections
	// ============================================================================
	
	// Configuration for a single viewport, or generally any 3D box.
	// Member layout and purpose directly matches the D3D11_VIEWPORT structure.
	struct Viewport
	{
		float TopLeftX;
		float TopLeftY;
		float Width;
		float Height;
		float MinDepth;
		float MaxDepth;

		Viewport() 
			: TopLeftX(0.f), TopLeftY(0.f), Width(0.f), Height(0.f), MinDepth(0.f), MaxDepth(1.f) 
		{ }

		Viewport(float x, float y, float w, float h, float minDepth = 0.f, float maxDepth = 1.f)
			: TopLeftX(x), TopLeftY(y), Width(w), Height(h), MinDepth(minDepth), MaxDepth(maxDepth)
		{ }

		Viewport(int x, int y, int w, int h, float minDepth = 0.f, float maxDepth = 1.f)
			: TopLeftX(float(x)), TopLeftY(float(y)), Width(float(w)), Height(float(h)), MinDepth(minDepth), MaxDepth(maxDepth)
		{ }
	};

	// Configuration for a single scissor rectangle, or generally any 2D rectangle.
	// Member layout and purpose directly matches the D3D11_RECT structure.
	struct ScissorRect
	{
		int Left;
		int Top;
		int Right;
		int Bottom;

		ScissorRect()
			: Left(0), Right(0), Top(0), Bottom(0)
		{ }

		ScissorRect(int left, int top, int right, int bottom)
			: Left(left), Top(top), Right(right), Bottom(bottom)
		{ }
	};

	// Vector with 2 floats.
	struct Float2
	{
		float x;
		float y;

		Float2()
			: x(0.f), y(0.f)
		{ }

		Float2(float scalar)
			: x(scalar), y(scalar)
		{ }

		Float2(float _x, float _y)
			: x(_x), y(_y)
		{ }
	};

	// Vector with 3 floats.
	struct Float3
	{
		float x;
		float y;
		float z;

		Float3()
			: x(0.f), y(0.f), z(0.f)
		{ }

		Float3(float scalar)
			: x(scalar), y(scalar), z(scalar)
		{ }

		Float3(float _x, float _y, float _z)
			: x(_x), y(_y), z(_z)
		{ }
	};

	// Vector with 4 floats.
	struct Float4
	{
		float x;
		float y;
		float z;
		float w;

		Float4()
			: x(0.f), y(0.f), z(0.f), w(0.f)
		{ }

		Float4(float scalar)
			: x(scalar), y(scalar), z(scalar), w(scalar)
		{ }

		Float4(float _x, float _y, float _z, float _w)
			: x(_x), y(_y), z(_z), w(_w)
		{ }
	};

	// Coefficients (scale and bias) for a linear mapping function.
	struct ScaleBias
	{
		float Scale;
		float Bias;

		ScaleBias()
			: Scale(1.f), Bias(0.f)
		{ }

		ScaleBias(float scale, float bias)
			: Scale(scale), Bias(bias)
		{ }
	};

	// Constant buffer data to supply the FastGS for culling primitives per-viewport
	// (matches corresponding struct in nv_vr.hlsli)
	// Note: needs to be 16-byte-aligned when placed in a constant buffer.
	// Split positions in NDC space (Y-up, [-1,1] range)
	struct FastGSCBData
	{
		float NDCSplitsX[2];
		float NDCSplitsY[2];
		float WarpLeft;
		float WarpRight;
		float WarpUp;
		float WarpDown;
	};

	// Constant buffer data to supply the UV-remapping helper functions
	// (matches corresponding struct in nv_vr.hlsli)
	struct RemapCBData
	{
		float		ClipToWindowSplitsX[2];
		float		ClipToWindowSplitsY[2];
		ScaleBias	ClipToWindowX[3];
		ScaleBias	ClipToWindowY[3];
		ScaleBias	ClipToWindowZ;

		float		WindowToClipSplitsX[2];
		float		WindowToClipSplitsY[2];
		ScaleBias	WindowToClipX[3];
		ScaleBias	WindowToClipY[3];
		ScaleBias	WindowToClipZ;

		Float2		BoundingRectOrigin;
		Float2		BoundingRectSize;
		Float2		BoundingRectSizeInv;

		float		Padding[2];
	};

	// Structure that holds data necessary to configure the viewport and scissor state for modified projection rendering.
	struct ProjectionViewports
	{
		enum { MaxCount = 16 };

		Viewport		Viewports[MaxCount];
		ScissorRect		Scissors[MaxCount];
		int				NumViewports;

		// Rectangle enclosing all the viewports and scissors (for sizing render targets, etc.)
		ScissorRect		BoundingRect;

		// The flattenedSize parameter that was passed to CalculateViewportsAndBufferData
		Float2			FlattenedSize;

		// Combines viewports and scissor rects from two similar structures, appending Right ones after Left ones.
		static void Merge(const ProjectionViewports& in_Left, const ProjectionViewports& in_Right, ProjectionViewports& out);

		// Computes a Cartesian product of horizontal and vertical sets of scissor boundaries and viewport equations,
		// producing a regular grid of viewports and scissor rectangles.
		static void MakeGrid(
			int Width,
			int Height,
			const int* ScissorBoundsX,
			const int* ScissorBoundsY,
			const ScaleBias* ViewportsX,
			const ScaleBias* ViewportsY,
			float MinDepth,
			float MaxDepth,
			ProjectionViewports& out);
	};

	template<Projection Type>
	struct Configuration;

	struct Data
	{
		ProjectionViewports			Viewports;
		FastGSCBData				FastGsCbData;
		RemapCBData					RemapCbData;
	};

	// ============================================================================
	// Functions
	// ============================================================================

	// Given a projection configuration, size of flat image, and a bounding box, calculates everything 
	// that is required to set up the GPU state and render using a modified projection.
	// Parameters:
	//  - flattenedSize is the size of final image after flattening, relative to which sizes and densities 
	//    are defined in projection configurations.
	//  - boundingBox is the position and maximum size of the projection in its render target. It does not 
	//    affect the size of the modified projection image, but it affects its placement, scissor rectangles, 
	//    and depth range (MinDepth and MaxDepth viewport parameters).
	//  - configuration is the configuration for the modified projection. Can be one of the presets.
	//  - ref_data is a write-only reference to structure that will contain all the calculated data.
	template<Projection Type>
	void CalculateViewportsAndBufferData(const Float2& flattenedSize, const Viewport& boundingBox, const Configuration<Type>& configuration, Data& ref_data);

	// Given a projection configuration and size of flat image, calculates the size of the modified projection.
	// This function can be used to determine the render target size that is required for the modified projection.
	template<Projection Type>
	Float2 CalculateProjectionSize(const Float2& flattenedSize, const Configuration<Type>& configuration);

	// Calculates a left-to-right mirrored version of the configuration. Useful for stereo rendering 
	// when the configuration is asymmetric. In this case, CalculateViewportsAndBufferData needs to be called twice:
	//  - once for the left eye with the original configuration or a preset, 
	//  - once for the right eye with the mirrored configuration and offset boundingBox (in case of side-by-side stereo rendering).
	template<Projection Type>
	void CalculateMirroredConfig(const Configuration<Type>&	sourceConfig, Configuration<Type>& ref_mirroredConfig);

	// Calculates the area, in pixels, of a modified projection, assuming that the application uses the scissor rectangles
	// provided by CalculateViewportsAndBufferData, and that pixel-level culling is used to discard everything outside 
	// of the LMS octagon when a lens matched projection is used.
	template<Projection Type>
	float CalculateRenderedArea(const Configuration<Type>& configuration, const ProjectionViewports& viewports);

	// Mapping from homogenous clip space coordinates to window-space pixel coordinates.
	// Input position is normalized, but not clipped against clip space bounds.
	template<Projection Type>
	Float3 MapClipToWindow(const Data& data, const Float4& clipPos);

	// Mapping from window-space pixel coordinates (including depth) to clip space coordinates.
	// Input position is not clipped against window bounds. 
	// Output position is normalized, i.e. has W component = 1.0, when "normalize" is set to true.
	template<Projection Type>
	Float4 MapWindowToClip(const Data& data, const Float3& windowPos, bool normalize = true);

	// Simplified version of MapClipToWindow that takes and returns Float2
	template<Projection Type>
	Float2 MapClipToWindow(const Data& data, const Float2& clipPos)
	{
		Float3 windowPos = MapClipToWindow(data, Float4{ clipPos.x, clipPos.y, 0.f, 1.f });
		return Float2{ windowPos.x, windowPos.y };
	}

	// Simplified version of MapWindowToClip that takes and returns Float2
	template<Projection Type>
	Float2 MapWindowToClip(const Data& data, const Float2& windowPos)
	{
		Float4 clipPos = MapWindowToClip(data, Float3{ windowPos.x, windowPos.y, 0.f }, true);
		return Float2{ clipPos.x, clipPos.y };
	}

	// ============================================================================
	// Planar projection specializations
	// ============================================================================

	template<>
	struct Configuration<Projection::PLANAR> { };

	namespace Planar
	{
		typedef Nv::VR::Configuration<Projection::PLANAR> Configuration;
	}

	// ============================================================================
	// Multi-res projection specializations (Multi-Res Shading, or MRS)
	// ============================================================================

	// Struct to define a multi-res viewport configuration
	template<>
	struct Configuration<Projection::MULTI_RES>
	{
		// Currently hardcoded for 3x3 viewports; later to be generalized
		enum { Width = 3, Height = 3, Count = Width * Height };

		// Size of the central viewport, ranging 0.01..1, where 1 is full original viewport size
		float CenterWidth;
		float CenterHeight;

		// Location of the central viewport, ranging 0..1, where 0.5 is the center of the screen
		float CenterX;
		float CenterY;

		// Pixel density scale factors: how much the linear pixel density is scaled within each
		// row and column (1.0 = full density)
		float DensityScaleX[Width];
		float DensityScaleY[Height];
	};

	namespace MultiRes
	{
		typedef Nv::VR::Configuration<Projection::MULTI_RES> Configuration;

		extern const Configuration Configuration_Balanced;
		extern const Configuration Configuration_Aggressive;
		extern const Configuration ConfigurationSet_OculusRift_CV1[3];
		extern const Configuration ConfigurationSet_HTC_Vive[3];
	}

	// ============================================================================
	// Lens-matched projection specializations (Lens-Matched Shading, or LMS)
	// ============================================================================

	template<>
	struct Configuration<Projection::LENS_MATCHED>
	{
		float WarpLeft;
		float WarpRight;
		float WarpUp;
		float WarpDown;

		float RelativeSizeLeft;
		float RelativeSizeRight;
		float RelativeSizeUp;
		float RelativeSizeDown;
	};

	namespace LensMatched
	{
		typedef Nv::VR::Configuration<Projection::LENS_MATCHED> Configuration;

		extern const Configuration ConfigurationSet_OculusRift_CV1[3];
		extern const Configuration ConfigurationSet_HTC_Vive[3];
	}
}
}