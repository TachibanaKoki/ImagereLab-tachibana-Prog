//----------------------------------------------------------------------------------
// File:        D3DHelper.h
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

struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11UnorderedAccessView;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=0; } }
#endif

#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif

template <typename T>
class D3DResource
{
public:
	D3DResource()
		: pResource(0)
		, pSRV(0)
		, pRTV(0)
		, pDSV(0)
		, pUAV(0)
	{ }

	D3DResource(const D3DResource& r)
	{
		pResource = r.pResource; if (pResource) pResource->AddRef();
		pSRV = r.pSRV; if (pSRV) pSRV->AddRef();
		pRTV = r.pRTV; if (pRTV) pRTV->AddRef();
		pDSV = r.pDSV; if (pDSV) pDSV->AddRef();
		pUAV = r.pUAV; if (pUAV) pUAV->AddRef();
	}

	void operator=(const D3DResource& r)
	{
		pResource = r.pResource; if (pResource) pResource->AddRef();
		pSRV = r.pSRV; if (pSRV) pSRV->AddRef();
		pRTV = r.pRTV; if (pRTV) pRTV->AddRef();
		pDSV = r.pDSV; if (pDSV) pDSV->AddRef();
		pUAV = r.pUAV; if (pUAV) pUAV->AddRef();
	}

	void Release()
	{
		SAFE_RELEASE(pResource);
		SAFE_RELEASE(pSRV);
		SAFE_RELEASE(pRTV);
		SAFE_RELEASE(pDSV);
		SAFE_RELEASE(pUAV);
	}

	~D3DResource()
	{
		Release();
	}

	T* pResource;
	ID3D11ShaderResourceView*   pSRV;
	ID3D11RenderTargetView*     pRTV;
	ID3D11DepthStencilView*     pDSV;
	ID3D11UnorderedAccessView*  pUAV;
};

typedef D3DResource<ID3D11Texture2D>    Texture2D;
typedef D3DResource<ID3D11Buffer>       Buffer;