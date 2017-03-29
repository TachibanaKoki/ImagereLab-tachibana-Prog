//----------------------------------------------------------------------------------
// File:        shader-slots.h
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

#ifndef SHADER_SLOTS_H
#define SHADER_SLOTS_H

// This file is included from both C++ and HLSL; it defines shared resource slot assignments

#ifdef __cplusplus
#	define CBREG(n)						n
#	define TEXREG(n)					n
#	define SAMPREG(n)					n
#else
#	define CBREG(n)						register(b##n)
#	define TEXREG(n)					register(t##n)
#	define SAMPREG(n)					register(s##n)
#endif

#define CB_FRAME						CBREG(0)
#define CB_VR							CBREG(1)

#define TEX_DIFFUSE						TEXREG(0)
#define TEX_NORMALS						TEXREG(1)
#define TEX_SPECULAR					TEXREG(2)
#define TEX_EMISSIVE					TEXREG(3)
#define TEX_SHADOW						TEXREG(4)

#define SAMP_DEFAULT					SAMPREG(0)
#define SAMP_SHADOW						SAMPREG(1)

#endif // SHADER_SLOTS_H
