#include "framework.h"

namespace Framework
{
	// RenderTarget implementation

	RenderTarget::RenderTarget()
	:	m_dims(makeint2(0)),
		m_sampleCount(0),
		m_format(DXGI_FORMAT_UNKNOWN)
	{
	}

	int BitsPerPixel(DXGI_FORMAT format)
	{
		static const int s_bitsPerPixel[] =
		{
			0,			// UNKNOWN
			128,		// R32G32B32A32_TYPELESS
			128,		// R32G32B32A32_FLOAT
			128,		// R32G32B32A32_UINT
			128,		// R32G32B32A32_SINT
			96,			// R32G32B32_TYPELESS
			96,			// R32G32B32_FLOAT
			96,			// R32G32B32_UINT
			96,			// R32G32B32_SINT
			64,			// R16G16B16A16_TYPELESS
			64,			// R16G16B16A16_FLOAT
			64,			// R16G16B16A16_UNORM
			64,			// R16G16B16A16_UINT
			64,			// R16G16B16A16_SNORM
			64,			// R16G16B16A16_SINT
			64,			// R32G32_TYPELESS
			64,			// R32G32_FLOAT
			64,			// R32G32_UINT
			64,			// R32G32_SINT
			64,			// R32G8X24_TYPELESS
			64,			// D32_FLOAT_S8X24_UINT
			64,			// R32_FLOAT_X8X24_TYPELESS
			64,			// X32_TYPELESS_G8X24_UINT
			32,			// R10G10B10A2_TYPELESS
			32,			// R10G10B10A2_UNORM
			32,			// R10G10B10A2_UINT
			32,			// R11G11B10_FLOAT
			32,			// R8G8B8A8_TYPELESS
			32,			// R8G8B8A8_UNORM
			32,			// R8G8B8A8_UNORM_SRGB
			32,			// R8G8B8A8_UINT
			32,			// R8G8B8A8_SNORM
			32,			// R8G8B8A8_SINT
			32,			// R16G16_TYPELESS
			32,			// R16G16_FLOAT
			32,			// R16G16_UNORM
			32,			// R16G16_UINT
			32,			// R16G16_SNORM
			32,			// R16G16_SINT
			32,			// R32_TYPELESS
			32,			// D32_FLOAT
			32,			// R32_FLOAT
			32,			// R32_UINT
			32,			// R32_SINT
			32,			// R24G8_TYPELESS
			32,			// D24_UNORM_S8_UINT
			32,			// R24_UNORM_X8_TYPELESS
			32,			// X24_TYPELESS_G8_UINT
			16,			// R8G8_TYPELESS
			16,			// R8G8_UNORM
			16,			// R8G8_UINT
			16,			// R8G8_SNORM
			16,			// R8G8_SINT
			16,			// R16_TYPELESS
			16,			// R16_FLOAT
			16,			// D16_UNORM
			16,			// R16_UNORM
			16,			// R16_UINT
			16,			// R16_SNORM
			16,			// R16_SINT
			8,			// R8_TYPELESS
			8,			// R8_UNORM
			8,			// R8_UINT
			8,			// R8_SNORM
			8,			// R8_SINT
			8,			// A8_UNORM
			1,			// R1_UNORM
			32,			// R9G9B9E5_SHAREDEXP
			16,			// R8G8_B8G8_UNORM
			16,			// G8R8_G8B8_UNORM
			4,			// BC1_TYPELESS
			4,			// BC1_UNORM
			4,			// BC1_UNORM_SRGB
			8,			// BC2_TYPELESS
			8,			// BC2_UNORM
			8,			// BC2_UNORM_SRGB
			8,			// BC3_TYPELESS
			8,			// BC3_UNORM
			8,			// BC3_UNORM_SRGB
			4,			// BC4_TYPELESS
			4,			// BC4_UNORM
			4,			// BC4_SNORM
			8,			// BC5_TYPELESS
			8,			// BC5_UNORM
			8,			// BC5_SNORM
			16,			// B5G6R5_UNORM
			16,			// B5G5R5A1_UNORM
			32,			// B8G8R8A8_UNORM
			32,			// B8G8R8X8_UNORM
			32,			// R10G10B10_XR_BIAS_A2_UNORM
			32,			// B8G8R8A8_TYPELESS
			32,			// B8G8R8A8_UNORM_SRGB
			32,			// B8G8R8X8_TYPELESS
			32,			// B8G8R8X8_UNORM_SRGB
			8,			// BC6H_TYPELESS
			8,			// BC6H_UF16
			8,			// BC6H_SF16
			8,			// BC7_TYPELESS
			8,			// BC7_UNORM
			8,			// BC7_UNORM_SRGB

						// NOTE: I don't know what are the bit depths for the video formats;
						// the MS docs don't seem to describe them in any detail.

			0,			// AYUV
			0,			// Y410
			0,			// Y416
			0,			// NV12
			0,			// P010
			0,			// P016
			0,			// 420_OPAQUE
			0,			// YUY2
			0,			// Y210
			0,			// Y216
			0,			// NV11
			0,			// AI44
			0,			// IA44

			8,			// P8
			16,			// A8P8
			16,			// B4G4R4A4_UNORM
		};

		if (uint(format) >= dim(s_bitsPerPixel))
		{
			WARN("Unexpected DXGI_FORMAT %d", format);
			return 0;
		}

		return s_bitsPerPixel[format];
	}

	DXGI_FORMAT FindTypelessFormat(DXGI_FORMAT format)
	{
		static const DXGI_FORMAT s_typelessFormat[] =
		{
			DXGI_FORMAT_UNKNOWN,					// UNKNOWN
			DXGI_FORMAT_R32G32B32A32_TYPELESS,		// R32G32B32A32_TYPELESS
			DXGI_FORMAT_R32G32B32A32_TYPELESS,		// R32G32B32A32_FLOAT
			DXGI_FORMAT_R32G32B32A32_TYPELESS,		// R32G32B32A32_UINT
			DXGI_FORMAT_R32G32B32A32_TYPELESS,		// R32G32B32A32_SINT
			DXGI_FORMAT_R32G32B32_TYPELESS,			// R32G32B32_TYPELESS
			DXGI_FORMAT_R32G32B32_TYPELESS,			// R32G32B32_FLOAT
			DXGI_FORMAT_R32G32B32_TYPELESS,			// R32G32B32_UINT
			DXGI_FORMAT_R32G32B32_TYPELESS,			// R32G32B32_SINT
			DXGI_FORMAT_R16G16B16A16_TYPELESS,		// R16G16B16A16_TYPELESS
			DXGI_FORMAT_R16G16B16A16_TYPELESS,		// R16G16B16A16_FLOAT
			DXGI_FORMAT_R16G16B16A16_TYPELESS,		// R16G16B16A16_UNORM
			DXGI_FORMAT_R16G16B16A16_TYPELESS,		// R16G16B16A16_UINT
			DXGI_FORMAT_R16G16B16A16_TYPELESS,		// R16G16B16A16_SNORM
			DXGI_FORMAT_R16G16B16A16_TYPELESS,		// R16G16B16A16_SINT
			DXGI_FORMAT_R32G32_TYPELESS,			// R32G32_TYPELESS
			DXGI_FORMAT_R32G32_TYPELESS,			// R32G32_FLOAT
			DXGI_FORMAT_R32G32_TYPELESS,			// R32G32_UINT
			DXGI_FORMAT_R32G32_TYPELESS,			// R32G32_SINT
			DXGI_FORMAT_R32G8X24_TYPELESS,			// R32G8X24_TYPELESS
			DXGI_FORMAT_R32G8X24_TYPELESS,			// D32_FLOAT_S8X24_UINT
			DXGI_FORMAT_R32G8X24_TYPELESS,			// R32_FLOAT_X8X24_TYPELESS
			DXGI_FORMAT_R32G8X24_TYPELESS,			// X32_TYPELESS_G8X24_UINT
			DXGI_FORMAT_R10G10B10A2_TYPELESS,		// R10G10B10A2_TYPELESS
			DXGI_FORMAT_R10G10B10A2_TYPELESS,		// R10G10B10A2_UNORM
			DXGI_FORMAT_R10G10B10A2_TYPELESS,		// R10G10B10A2_UINT
			DXGI_FORMAT_UNKNOWN,					// R11G11B10_FLOAT
			DXGI_FORMAT_R8G8B8A8_TYPELESS,			// R8G8B8A8_TYPELESS
			DXGI_FORMAT_R8G8B8A8_TYPELESS,			// R8G8B8A8_UNORM
			DXGI_FORMAT_R8G8B8A8_TYPELESS,			// R8G8B8A8_UNORM_SRGB
			DXGI_FORMAT_R8G8B8A8_TYPELESS,			// R8G8B8A8_UINT
			DXGI_FORMAT_R8G8B8A8_TYPELESS,			// R8G8B8A8_SNORM
			DXGI_FORMAT_R8G8B8A8_TYPELESS,			// R8G8B8A8_SINT
			DXGI_FORMAT_R16G16_TYPELESS,			// R16G16_TYPELESS
			DXGI_FORMAT_R16G16_TYPELESS,			// R16G16_FLOAT
			DXGI_FORMAT_R16G16_TYPELESS,			// R16G16_UNORM
			DXGI_FORMAT_R16G16_TYPELESS,			// R16G16_UINT
			DXGI_FORMAT_R16G16_TYPELESS,			// R16G16_SNORM
			DXGI_FORMAT_R16G16_TYPELESS,			// R16G16_SINT
			DXGI_FORMAT_R32_TYPELESS,				// R32_TYPELESS
			DXGI_FORMAT_R32_TYPELESS,				// D32_FLOAT
			DXGI_FORMAT_R32_TYPELESS,				// R32_FLOAT
			DXGI_FORMAT_R32_TYPELESS,				// R32_UINT
			DXGI_FORMAT_R32_TYPELESS,				// R32_SINT
			DXGI_FORMAT_R24G8_TYPELESS,				// R24G8_TYPELESS
			DXGI_FORMAT_R24G8_TYPELESS,				// D24_UNORM_S8_UINT
			DXGI_FORMAT_R24G8_TYPELESS,				// R24_UNORM_X8_TYPELESS
			DXGI_FORMAT_R24G8_TYPELESS,				// X24_TYPELESS_G8_UINT
			DXGI_FORMAT_R8G8_TYPELESS,				// R8G8_TYPELESS
			DXGI_FORMAT_R8G8_TYPELESS,				// R8G8_UNORM
			DXGI_FORMAT_R8G8_TYPELESS,				// R8G8_UINT
			DXGI_FORMAT_R8G8_TYPELESS,				// R8G8_SNORM
			DXGI_FORMAT_R8G8_TYPELESS,				// R8G8_SINT
			DXGI_FORMAT_R16_TYPELESS,				// R16_TYPELESS
			DXGI_FORMAT_R16_TYPELESS,				// R16_FLOAT
			DXGI_FORMAT_R16_TYPELESS,				// D16_UNORM
			DXGI_FORMAT_R16_TYPELESS,				// R16_UNORM
			DXGI_FORMAT_R16_TYPELESS,				// R16_UINT
			DXGI_FORMAT_R16_TYPELESS,				// R16_SNORM
			DXGI_FORMAT_R16_TYPELESS,				// R16_SINT
			DXGI_FORMAT_R8_TYPELESS,				// R8_TYPELESS
			DXGI_FORMAT_R8_TYPELESS,				// R8_UNORM
			DXGI_FORMAT_R8_TYPELESS,				// R8_UINT
			DXGI_FORMAT_R8_TYPELESS,				// R8_SNORM
			DXGI_FORMAT_R8_TYPELESS,				// R8_SINT
			DXGI_FORMAT_R8_TYPELESS,				// A8_UNORM
			DXGI_FORMAT_UNKNOWN,					// R1_UNORM
			DXGI_FORMAT_UNKNOWN,					// R9G9B9E5_SHAREDEXP
			DXGI_FORMAT_UNKNOWN,					// R8G8_B8G8_UNORM
			DXGI_FORMAT_UNKNOWN,					// G8R8_G8B8_UNORM
			DXGI_FORMAT_BC1_TYPELESS,				// BC1_TYPELESS
			DXGI_FORMAT_BC1_TYPELESS,				// BC1_UNORM
			DXGI_FORMAT_BC1_TYPELESS,				// BC1_UNORM_SRGB
			DXGI_FORMAT_BC2_TYPELESS,				// BC2_TYPELESS
			DXGI_FORMAT_BC2_TYPELESS,				// BC2_UNORM
			DXGI_FORMAT_BC2_TYPELESS,				// BC2_UNORM_SRGB
			DXGI_FORMAT_BC3_TYPELESS,				// BC3_TYPELESS
			DXGI_FORMAT_BC3_TYPELESS,				// BC3_UNORM
			DXGI_FORMAT_BC3_TYPELESS,				// BC3_UNORM_SRGB
			DXGI_FORMAT_BC4_TYPELESS,				// BC4_TYPELESS
			DXGI_FORMAT_BC4_TYPELESS,				// BC4_UNORM
			DXGI_FORMAT_BC4_TYPELESS,				// BC4_SNORM
			DXGI_FORMAT_BC5_TYPELESS,				// BC5_TYPELESS
			DXGI_FORMAT_BC5_TYPELESS,				// BC5_UNORM
			DXGI_FORMAT_BC5_TYPELESS,				// BC5_SNORM
			DXGI_FORMAT_UNKNOWN,					// B5G6R5_UNORM
			DXGI_FORMAT_UNKNOWN,					// B5G5R5A1_UNORM
			DXGI_FORMAT_UNKNOWN,					// B8G8R8A8_UNORM
			DXGI_FORMAT_UNKNOWN,					// B8G8R8X8_UNORM
			DXGI_FORMAT_UNKNOWN,					// R10G10B10_XR_BIAS_A2_UNORM
			DXGI_FORMAT_B8G8R8A8_TYPELESS,			// B8G8R8A8_TYPELESS
			DXGI_FORMAT_B8G8R8A8_TYPELESS,			// B8G8R8A8_UNORM_SRGB
			DXGI_FORMAT_B8G8R8X8_TYPELESS,			// B8G8R8X8_TYPELESS
			DXGI_FORMAT_B8G8R8X8_TYPELESS,			// B8G8R8X8_UNORM_SRGB
			DXGI_FORMAT_BC6H_TYPELESS,				// BC6H_TYPELESS
			DXGI_FORMAT_BC6H_TYPELESS,				// BC6H_UF16
			DXGI_FORMAT_BC6H_TYPELESS,				// BC6H_SF16
			DXGI_FORMAT_BC7_TYPELESS,				// BC7_TYPELESS
			DXGI_FORMAT_BC7_TYPELESS,				// BC7_UNORM
			DXGI_FORMAT_BC7_TYPELESS,				// BC7_UNORM_SRGB
			DXGI_FORMAT_UNKNOWN,					// AYUV
			DXGI_FORMAT_UNKNOWN,					// Y410
			DXGI_FORMAT_UNKNOWN,					// Y416
			DXGI_FORMAT_UNKNOWN,					// NV12
			DXGI_FORMAT_UNKNOWN,					// P010
			DXGI_FORMAT_UNKNOWN,					// P016
			DXGI_FORMAT_UNKNOWN,					// 420_OPAQUE
			DXGI_FORMAT_UNKNOWN,					// YUY2
			DXGI_FORMAT_UNKNOWN,					// Y210
			DXGI_FORMAT_UNKNOWN,					// Y216
			DXGI_FORMAT_UNKNOWN,					// NV11
			DXGI_FORMAT_UNKNOWN,					// AI44
			DXGI_FORMAT_UNKNOWN,					// IA44
			DXGI_FORMAT_UNKNOWN,					// P8
			DXGI_FORMAT_UNKNOWN,					// A8P8
			DXGI_FORMAT_UNKNOWN,					// B4G4R4A4_UNORM
		};

		if (uint(format) >= dim(s_typelessFormat))
		{
			WARN("Unexpected DXGI_FORMAT %d", format);
			return DXGI_FORMAT_UNKNOWN;
		}

		return s_typelessFormat[format];
	}

	const char * NameOfFormat(DXGI_FORMAT format)
	{
		static const char * s_names[] =
		{
			"UNKNOWN",
			"R32G32B32A32_TYPELESS",
			"R32G32B32A32_FLOAT",
			"R32G32B32A32_UINT",
			"R32G32B32A32_SINT",
			"R32G32B32_TYPELESS",
			"R32G32B32_FLOAT",
			"R32G32B32_UINT",
			"R32G32B32_SINT",
			"R16G16B16A16_TYPELESS",
			"R16G16B16A16_FLOAT",
			"R16G16B16A16_UNORM",
			"R16G16B16A16_UINT",
			"R16G16B16A16_SNORM",
			"R16G16B16A16_SINT",
			"R32G32_TYPELESS",
			"R32G32_FLOAT",
			"R32G32_UINT",
			"R32G32_SINT",
			"R32G8X24_TYPELESS",
			"D32_FLOAT_S8X24_UINT",
			"R32_FLOAT_X8X24_TYPELESS",
			"X32_TYPELESS_G8X24_UINT",
			"R10G10B10A2_TYPELESS",
			"R10G10B10A2_UNORM",
			"R10G10B10A2_UINT",
			"R11G11B10_FLOAT",
			"R8G8B8A8_TYPELESS",
			"R8G8B8A8_UNORM",
			"R8G8B8A8_UNORM_SRGB",
			"R8G8B8A8_UINT",
			"R8G8B8A8_SNORM",
			"R8G8B8A8_SINT",
			"R16G16_TYPELESS",
			"R16G16_FLOAT",
			"R16G16_UNORM",
			"R16G16_UINT",
			"R16G16_SNORM",
			"R16G16_SINT",
			"R32_TYPELESS",
			"D32_FLOAT",
			"R32_FLOAT",
			"R32_UINT",
			"R32_SINT",
			"R24G8_TYPELESS",
			"D24_UNORM_S8_UINT",
			"R24_UNORM_X8_TYPELESS",
			"X24_TYPELESS_G8_UINT",
			"R8G8_TYPELESS",
			"R8G8_UNORM",
			"R8G8_UINT",
			"R8G8_SNORM",
			"R8G8_SINT",
			"R16_TYPELESS",
			"R16_FLOAT",
			"D16_UNORM",
			"R16_UNORM",
			"R16_UINT",
			"R16_SNORM",
			"R16_SINT",
			"R8_TYPELESS",
			"R8_UNORM",
			"R8_UINT",
			"R8_SNORM",
			"R8_SINT",
			"A8_UNORM",
			"R1_UNORM",
			"R9G9B9E5_SHAREDEXP",
			"R8G8_B8G8_UNORM",
			"G8R8_G8B8_UNORM",
			"BC1_TYPELESS",
			"BC1_UNORM",
			"BC1_UNORM_SRGB",
			"BC2_TYPELESS",
			"BC2_UNORM",
			"BC2_UNORM_SRGB",
			"BC3_TYPELESS",
			"BC3_UNORM",
			"BC3_UNORM_SRGB",
			"BC4_TYPELESS",
			"BC4_UNORM",
			"BC4_SNORM",
			"BC5_TYPELESS",
			"BC5_UNORM",
			"BC5_SNORM",
			"B5G6R5_UNORM",
			"B5G5R5A1_UNORM",
			"B8G8R8A8_UNORM",
			"B8G8R8X8_UNORM",
			"R10G10B10_XR_BIAS_A2_UNORM",
			"B8G8R8A8_TYPELESS",
			"B8G8R8A8_UNORM_SRGB",
			"B8G8R8X8_TYPELESS",
			"B8G8R8X8_UNORM_SRGB",
			"BC6H_TYPELESS",
			"BC6H_UF16",
			"BC6H_SF16",
			"BC7_TYPELESS",
			"BC7_UNORM",
			"BC7_UNORM_SRGB",
			"AYUV",
			"Y410",
			"Y416",
			"NV12",
			"P010",
			"P016",
			"420_OPAQUE",
			"YUY2",
			"Y210",
			"Y216",
			"NV11",
			"AI44",
			"IA44",
			"P8",
			"A8P8",
			"B4G4R4A4_UNORM",
		};

		if (uint(format) >= dim(s_names))
		{
			WARN("Unexpected DXGI_FORMAT %d", format);
			return "UNKNOWN";
		}

		return s_names[format];
	}

	void WriteBMPToMemory(
		const byte4 * pPixels,
		int2_arg dims,
		std::vector<byte> * pDataOut)
	{
		ASSERT_ERR(pPixels);
		ASSERT_ERR(all(dims > 0));
		ASSERT_ERR(pDataOut);

		// Compose the .bmp headers
		BITMAPFILEHEADER bfh =
		{
			0x4d42,		// "BM"
			0, 0, 0,
			sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER),
		};
		BITMAPINFOHEADER bih =
		{
			sizeof(BITMAPINFOHEADER),
			dims.x, -dims.y,	// Negative height makes it go top-down
			1, 32, BI_RGB,
		};

		// Allocate a buffer to compose the .bmp file
		int imageSizeBytes = dims.x * dims.y * sizeof(byte4);
		int totalSizeBytes = sizeof(bfh) + sizeof(bih) + imageSizeBytes;
		pDataOut->resize(totalSizeBytes);

		// Compose the file
		memcpy(&(*pDataOut)[0], &bfh, sizeof(bfh));
		memcpy(&(*pDataOut)[sizeof(bfh)], &bih, sizeof(bih));
		for (int i = 0, c = dims.x * dims.y; i < c; ++i)
		{
			byte4 rgba = pPixels[i];
			byte4 bgra = { rgba.b, rgba.g, rgba.r, rgba.a };
			*(byte4 *)&(*pDataOut)[sizeof(bfh) + sizeof(bih) + i * sizeof(byte4)] = bgra;
		}
	}

	bool WriteBMPToFile(
		const byte4 * pPixels,
		int2_arg dims,
		const char * path)
	{
		ASSERT_ERR(pPixels);
		ASSERT_ERR(all(dims > 0));
		ASSERT_ERR(path);

		FILE * pFile = nullptr;
		if (fopen_s(&pFile, path, "wb") != 0)
			return false;

		std::vector<byte> buffer;
		WriteBMPToMemory(pPixels, dims, &buffer);
		if (fwrite(&buffer[0], buffer.size(), 1, pFile) < 1)
		{
			fclose(pFile);
			return false;
		}

		fclose(pFile);
		return true;
	}

	void RenderTarget::Init(
		ID3D11Device * pDevice,
		int2_arg dims,
		DXGI_FORMAT format,
		int sampleCount, /* = 1 */
		int flags /* = RTFLAG_Default */,
		int arraySize /* = 1 */)
	{
		ASSERT_ERR(pDevice);

		// Always map the format to its typeless version, if possible;
		// enables views of other formats to be created if desired
		DXGI_FORMAT formatTex = FindTypelessFormat(format);
		if (formatTex == DXGI_FORMAT_UNKNOWN)
			formatTex = format;

		D3D11_TEXTURE2D_DESC texDesc =
		{
			UINT(dims.x), UINT(dims.y), 1, UINT(arraySize),
			formatTex,
			{ UINT(sampleCount), 0 },
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			0, 0,
		};
		if (flags & RTFLAG_EnableUAV)
		{
			texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		CHECK_D3D(pDevice->CreateTexture2D(&texDesc, nullptr, &m_pTex));

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc =
		{
			format,
			(sampleCount > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D,
		};
		if (arraySize > 1)
		{
			if (sampleCount > 1)
			{
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
				rtvDesc.Texture2DMSArray.ArraySize = arraySize;
				rtvDesc.Texture2DMSArray.FirstArraySlice = 0;
			}
			else
			{
				rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.ArraySize = arraySize;
				rtvDesc.Texture2DArray.FirstArraySlice = 0;
				rtvDesc.Texture2DArray.MipSlice = 0;
			}
		}
		CHECK_D3D(pDevice->CreateRenderTargetView(m_pTex, &rtvDesc, &m_pRtv));
		
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
			format,
			(sampleCount > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D,
		};
		if (sampleCount == 1)
			srvDesc.Texture2D.MipLevels = 1;
		if (arraySize > 1)
		{
			if (sampleCount > 1)
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
				srvDesc.Texture2DMSArray.ArraySize = arraySize;
				srvDesc.Texture2DMSArray.FirstArraySlice = 0;
			}
			else
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Texture2DArray.ArraySize = arraySize;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.MipLevels = 1;
				srvDesc.Texture2DArray.MostDetailedMip = 0;
			}
		}
		CHECK_D3D(pDevice->CreateShaderResourceView(m_pTex, &srvDesc, &m_pSrv));
		
		if (flags & RTFLAG_EnableUAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = { format, D3D11_UAV_DIMENSION_TEXTURE2D, };
			CHECK_D3D(pDevice->CreateUnorderedAccessView(m_pTex, &uavDesc, &m_pUav));
		}

		m_dims = dims;
		m_sampleCount = sampleCount;
		m_arraySize = arraySize;
		m_format = format;
	}

	void RenderTarget::Reset()
	{
		m_pTex.release();
		m_pRtv.release();
		m_pSrv.release();
		m_pUav.release();
		m_dims = makeint2(0);
		m_sampleCount = 0;
		m_format = DXGI_FORMAT_UNKNOWN;
	}

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT d3dViewport = { 0.0f, 0.0f, float(m_dims.x), float(m_dims.y), 0.0f, 1.0f, };
		pCtx->RSSetViewports(1, &d3dViewport);
		D3D11_RECT rect = { 0, 0, m_dims.x, m_dims.y };
		pCtx->RSSetScissorRects(1, &rect);
	}

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx, box2_arg viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.m_mins.x, viewport.m_mins.y,
			viewport.diagonal().x, viewport.diagonal().y,
			0.0f, 1.0f,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void RenderTarget::Bind(ID3D11DeviceContext * pCtx, box3_arg viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(1, &m_pRtv, nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.m_mins.x, viewport.m_mins.y,
			viewport.diagonal().x, viewport.diagonal().y,
			viewport.m_mins.z, viewport.m_maxs.z,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void RenderTarget::Readback(
		ID3D11DeviceContext * pCtx,
		void * pDataOut)
	{
		ASSERT_ERR(m_pTex);
		ASSERT_ERR_MSG(m_sampleCount == 1, "D3D11 doesn't support readback of multisampled render targets");
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pDataOut);

		comptr<ID3D11Device> pDevice;
		pCtx->GetDevice(&pDevice);

		// Create a staging resource
		D3D11_TEXTURE2D_DESC texDesc =
		{
			UINT(m_dims.x), UINT(m_dims.y), 1, 1,
			m_format,
			{ 1, 0 },
			D3D11_USAGE_STAGING,
			0,
			D3D11_CPU_ACCESS_READ,
			0,
		};
		comptr<ID3D11Texture2D> pTexStaging;
		pDevice->CreateTexture2D(&texDesc, nullptr, &pTexStaging);

		// Copy the data to the staging resource
		pCtx->CopyResource(pTexStaging, m_pTex);

		// Map the staging resource
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		CHECK_D3D(pCtx->Map(pTexStaging, 0, D3D11_MAP_READ, 0, &mapped));

		// Copy the data out row by row, in case the pitch is different
		int rowSize = m_dims.x * BitsPerPixel(m_format) / 8;
		ASSERT_ERR(mapped.RowPitch >= UINT(rowSize));
		for (int y = 0; y < m_dims.y; ++y)
		{
			memcpy(
				advanceBytes(pDataOut, y * rowSize),
				advanceBytes(mapped.pData, y * mapped.RowPitch),
				rowSize);
		}

		pCtx->Unmap(pTexStaging, 0);
	}



	// DepthStencilTarget implementation

	struct DepthStencilFormats
	{
		DXGI_FORMAT		m_formatTypeless;
		DXGI_FORMAT		m_formatDsv;
		DXGI_FORMAT		m_formatSrvDepth;
		DXGI_FORMAT		m_formatSrvStencil;
	};

	static const DepthStencilFormats s_depthStencilFormats[] =
	{
		// 32-bit float depth + 8-bit stencil
		{
			DXGI_FORMAT_R32G8X24_TYPELESS,
			DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
			DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
			DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
		},
		// 32-bit float depth, no stencil
		{
			DXGI_FORMAT_R32_TYPELESS,
			DXGI_FORMAT_D32_FLOAT,
			DXGI_FORMAT_R32_FLOAT,
			DXGI_FORMAT_UNKNOWN,
		},
		// 24-bit fixed-point depth + 8-bit stencil
		{
			DXGI_FORMAT_R24G8_TYPELESS,
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
			DXGI_FORMAT_X24_TYPELESS_G8_UINT,
		},
		// 16-bit fixed-point depth, no stencil
		{
			DXGI_FORMAT_R16_TYPELESS,
			DXGI_FORMAT_D16_UNORM,
			DXGI_FORMAT_R16_UNORM,
			DXGI_FORMAT_UNKNOWN,
		},
	};

	DepthStencilTarget::DepthStencilTarget()
	:	m_dims(makeint2(0)),
		m_sampleCount(0),
		m_formatDsv(DXGI_FORMAT_UNKNOWN),
		m_formatSrvDepth(DXGI_FORMAT_UNKNOWN),
		m_formatSrvStencil(DXGI_FORMAT_UNKNOWN)
	{
	}

	void DepthStencilTarget::Init(
		ID3D11Device * pDevice,
		int2_arg dims,
		DXGI_FORMAT format,
		int sampleCount, /* = 1 */
		int flags /* = DSFLAG_Default */,
		int arraySize /* = 1 */)
	{
		ASSERT_ERR(pDevice);

		// Check that the format matches one of our known depth-stencil formats
		DepthStencilFormats formats = {};
		for (int i = 0; i < dim(s_depthStencilFormats); ++i)
		{
			if (s_depthStencilFormats[i].m_formatDsv == format)
				formats = s_depthStencilFormats[i];
		}
		ASSERT_ERR_MSG(
			formats.m_formatDsv == format,
			"Depth-stencil format must be one of those listed in s_depthStencilFormats; found %s instead",
			NameOfFormat(format));

		D3D11_TEXTURE2D_DESC texDesc =
		{
			UINT(dims.x), UINT(dims.y), 1, UINT(arraySize),
			formats.m_formatTypeless,
			{ UINT(sampleCount), 0 },
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
			0, 0,
		};
		if (flags & RTFLAG_EnableUAV)
		{
			texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		CHECK_D3D(pDevice->CreateTexture2D(&texDesc, nullptr, &m_pTex));

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc =
		{
			formats.m_formatDsv,
			(sampleCount > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D,
		};
		if (arraySize > 1)
		{
			if (sampleCount > 1)
			{
				dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
				dsvDesc.Texture2DMSArray.ArraySize = arraySize;
				dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
			}
			else
			{
				dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray.ArraySize = arraySize;
				dsvDesc.Texture2DArray.FirstArraySlice = 0;
				dsvDesc.Texture2DArray.MipSlice = 0;
			}
		}
		CHECK_D3D(pDevice->CreateDepthStencilView(m_pTex, &dsvDesc, &m_pDsv));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
		{
			formats.m_formatSrvDepth,
			(sampleCount > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D,
		};
		if (sampleCount == 1)
			srvDesc.Texture2D.MipLevels = 1;
		if (arraySize > 1)
		{
			if (sampleCount > 1)
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
				srvDesc.Texture2DMSArray.ArraySize = arraySize;
				srvDesc.Texture2DMSArray.FirstArraySlice = 0;
			}
			else
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Texture2DArray.ArraySize = arraySize;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.MipLevels = 1;
				srvDesc.Texture2DArray.MostDetailedMip = 0;
			}
		}
		CHECK_D3D(pDevice->CreateShaderResourceView(m_pTex, &srvDesc, &m_pSrvDepth));

		if (formats.m_formatSrvStencil != DXGI_FORMAT_UNKNOWN)
		{
			srvDesc.Format = formats.m_formatSrvStencil;
			CHECK_D3D(pDevice->CreateShaderResourceView(m_pTex, &srvDesc, &m_pSrvStencil));
		}

		if (flags & DSFLAG_EnableUAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = { formats.m_formatSrvDepth, D3D11_UAV_DIMENSION_TEXTURE2D, };
			CHECK_D3D(pDevice->CreateUnorderedAccessView(m_pTex, &uavDesc, &m_pUavDepth));

			if (formats.m_formatSrvStencil != DXGI_FORMAT_UNKNOWN)
			{
				uavDesc.Format = formats.m_formatSrvStencil;
				CHECK_D3D(pDevice->CreateUnorderedAccessView(m_pTex, &uavDesc, &m_pUavStencil));
			}
		}

		m_dims = dims;
		m_sampleCount = sampleCount;
		m_arraySize = arraySize;
		m_formatDsv = format;
		m_formatSrvDepth = formats.m_formatSrvDepth;
		m_formatSrvStencil = formats.m_formatSrvStencil;
	}

	void DepthStencilTarget::Reset()
	{
		m_pTex.release();
		m_pDsv.release();
		m_pSrvDepth.release();
		m_pSrvStencil.release();
		m_pUavDepth.release();
		m_pUavStencil.release();
		m_dims = makeint2(0);
		m_sampleCount = 0;
		m_formatDsv = DXGI_FORMAT_UNKNOWN;
		m_formatSrvDepth = DXGI_FORMAT_UNKNOWN;
		m_formatSrvStencil = DXGI_FORMAT_UNKNOWN;
	}

	void DepthStencilTarget::Bind(ID3D11DeviceContext * pCtx)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(0, nullptr, m_pDsv);
		D3D11_VIEWPORT d3dViewport = { 0.0f, 0.0f, float(m_dims.x), float(m_dims.y), 0.0f, 1.0f, };
		pCtx->RSSetViewports(1, &d3dViewport);
		D3D11_RECT rect = { 0, 0, m_dims.x, m_dims.y };
		pCtx->RSSetScissorRects(1, &rect);
	}

	void DepthStencilTarget::Bind(ID3D11DeviceContext * pCtx, box2_arg viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(0, nullptr, m_pDsv);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.m_mins.x, viewport.m_mins.y,
			viewport.diagonal().x, viewport.diagonal().y,
			0.0f, 1.0f,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void DepthStencilTarget::Bind(ID3D11DeviceContext * pCtx, box3_arg viewport)
	{
		ASSERT_ERR(pCtx);

		pCtx->OMSetRenderTargets(0, nullptr, m_pDsv);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.m_mins.x, viewport.m_mins.y,
			viewport.diagonal().x, viewport.diagonal().y,
			viewport.m_mins.z, viewport.m_maxs.z,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void DepthStencilTarget::Readback(
		ID3D11DeviceContext * pCtx,
		void * pDataOut)
	{
		ASSERT_ERR(m_pTex);
		ASSERT_ERR_MSG(m_sampleCount == 1, "D3D11 doesn't support readback of multisampled render targets");
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pDataOut);

		comptr<ID3D11Device> pDevice;
		pCtx->GetDevice(&pDevice);

		// Create a staging resource
		D3D11_TEXTURE2D_DESC texDesc =
		{
			UINT(m_dims.x), UINT(m_dims.y), 1, 1,
			m_formatDsv,
			{ 1, 0 },
			D3D11_USAGE_STAGING,
			0,
			D3D11_CPU_ACCESS_READ,
			0,
		};
		comptr<ID3D11Texture2D> pTexStaging;
		pDevice->CreateTexture2D(&texDesc, nullptr, &pTexStaging);

		// Copy the data to the staging resource
		pCtx->CopyResource(pTexStaging, m_pTex);

		// Map the staging resource
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		CHECK_D3D(pCtx->Map(pTexStaging, 0, D3D11_MAP_READ, 0, &mapped));

		// Copy the data out row by row, in case the pitch is different
		int rowSize = m_dims.x * BitsPerPixel(m_formatDsv) / 8;
		ASSERT_ERR(mapped.RowPitch >= UINT(rowSize));
		for (int y = 0; y < m_dims.y; ++y)
		{
			memcpy(
				advanceBytes(pDataOut, y * rowSize),
				advanceBytes(mapped.pData, y * mapped.RowPitch),
				rowSize);
		}

		pCtx->Unmap(pTexStaging, 0);
	}



	// Utility functions for binding multiple render targets

	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pRt);
		ASSERT_ERR(pRt->m_pRtv);
		if (pDst)
			ASSERT_ERR(all(pRt->m_dims == pDst->m_dims));

		pCtx->OMSetRenderTargets(1, &pRt->m_pRtv, pDst ? pDst->m_pDsv : nullptr);
		D3D11_VIEWPORT d3dViewport = { 0.0f, 0.0f, float(pRt->m_dims.x), float(pRt->m_dims.y), 0.0f, 1.0f, };
		pCtx->RSSetViewports(1, &d3dViewport);
		D3D11_RECT rect = { 0, 0, pRt->m_dims.x, pRt->m_dims.y };
		pCtx->RSSetScissorRects(1, &rect);
	}

	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst, box2_arg viewport)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pRt);
		ASSERT_ERR(pRt->m_pRtv);
		if (pDst)
			ASSERT_ERR(all(pRt->m_dims == pDst->m_dims));

		pCtx->OMSetRenderTargets(1, &pRt->m_pRtv, pDst ? pDst->m_pDsv : nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.m_mins.x, viewport.m_mins.y,
			viewport.diagonal().x, viewport.diagonal().y,
			0.0f, 1.0f,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void BindRenderTargets(ID3D11DeviceContext * pCtx, RenderTarget * pRt, DepthStencilTarget * pDst, box3_arg viewport)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pRt);
		ASSERT_ERR(pRt->m_pRtv);
		if (pDst)
			ASSERT_ERR(all(pRt->m_dims == pDst->m_dims));

		pCtx->OMSetRenderTargets(1, &pRt->m_pRtv, pDst ? pDst->m_pDsv : nullptr);
		D3D11_VIEWPORT d3dViewport =
		{
			viewport.m_mins.x, viewport.m_mins.y,
			viewport.diagonal().x, viewport.diagonal().y,
			viewport.m_mins.z, viewport.m_maxs.z,
		};
		pCtx->RSSetViewports(1, &d3dViewport);
	}

	void BindMultipleRenderTargets(ID3D11DeviceContext * pCtx, UINT NumRTs, RenderTarget * pRts, DepthStencilTarget * pDst)
	{
		ASSERT_ERR(pCtx);
		ID3D11RenderTargetView* pRTVs[8];
		ASSERT_ERR(pRts);

		for (UINT n = 0; n < NumRTs; n++)
		{
			ASSERT_ERR(pRts[n].m_pRtv);
			if (pDst)
				ASSERT_ERR(all(pRts[n].m_dims == pDst->m_dims));
			pRTVs[n] = pRts[n].m_pRtv;
		}
		
		pCtx->OMSetRenderTargets(NumRTs, pRTVs, pDst ? pDst->m_pDsv : nullptr);
		D3D11_VIEWPORT d3dViewport = { 0.0f, 0.0f, float(pRts[0].m_dims.x), float(pRts[0].m_dims.y), 0.0f, 1.0f, };
		pCtx->RSSetViewports(1, &d3dViewport);
		D3D11_RECT rect = { 0, 0, pRts[0].m_dims.x, pRts[0].m_dims.y };
		pCtx->RSSetScissorRects(1, &rect);
	}


	// Helper functions for saving out screenshots of render targets

	bool WriteRenderTargetToBMP(
		ID3D11DeviceContext * pCtx,
		RenderTarget * pRt,
		const char * path)
	{
		ASSERT_ERR(pCtx);
		ASSERT_ERR(pRt);
		ASSERT_ERR(all(pRt->m_dims > 0));
		ASSERT_ERR(path);

		// Currently the texture must be in RGBA8 format and can't be multisampled
		ASSERT_ERR(pRt->m_format == DXGI_FORMAT_R8G8B8A8_UNORM || pRt->m_format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		ASSERT_ERR(pRt->m_sampleCount == 1);

		std::vector<byte4> pixels(pRt->m_dims.x * pRt->m_dims.y);
		pRt->Readback(pCtx, &pixels[0]);

		return WriteBMPToFile(&pixels[0], pRt->m_dims, path);
	}
}
