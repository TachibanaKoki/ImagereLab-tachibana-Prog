//----------------------------------------------------------------------------------
// File:        demo_dx11.cpp
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

#define USE_GPU_TIMER

#include <framework.h>
#include <AntTweakBar.h>
#include <openvr.h>
#include <nvapi.h>
#include <nvShaderExtnEnums.h>
#include <nv_vr.h>
#include "shaders/shader-slots.h"
#include "Scene.h"
#include "json/reader.h"
#include <fstream>
#include <Shlwapi.h>
#include "ShaderFactory.h"
#include "ShaderState.h"
#include <thread>

#include <OVR_CAPI_D3D.h>

using namespace util;
using namespace Framework;

#pragma region VR defines

// Define error checkers for Oculus APIs
#define CHECK_OVR(f) \
		{ \
			ovrResult result = f; \
			if (OVR_FAILURE(result)) \
			{ \
				ovrErrorInfo errorInfo; \
				ovr_GetLastErrorInfo(&errorInfo); \
				ERR("LibOVR call failed with error code: %d\nFailed call: %s\nError message: %s", result, #f, errorInfo.ErrorString); \
			} \
		}

#define CHECK_OVR_WARN(f) \
		{ \
			ovrResult result = f; \
			if (OVR_FAILURE(result)) \
			{ \
				ovrErrorInfo errorInfo; \
				ovr_GetLastErrorInfo(&errorInfo); \
				WARN("LibOVR call failed with error code: %d\nFailed call: %s\nError message: %s", result, #f, errorInfo.ErrorString); \
			} \
		}

// Define error checkers for OpenVR APIs
#define CHECK_OPENVR_WARN(f) \
		{ \
			vr::EVRCompositorError result = f; \
			ASSERT_WARN_MSG(result == vr::VRCompositorError_None, "OpenVR call failed with error code: %d\nFailed call: %s", result, #f); \
		}
#pragma endregion


// Rendering settings - global for simplicity

float3						g_vecDirectionalLight		= normalize(makefloat3(-2.0f, 10.0f, 1.5f));
rgb							g_rgbDirectionalLight		= makergb(1.1f, 1.0f, 0.7f);
rgb							g_rgbSky					= makergb(0.f, 0.f, 0.f);
float						g_normalOffsetShadow		= 1e-5f;	// meters
float						g_shadowSharpening			= 1.0f;
float						g_zNear						= 0.1f;		// meters
float						g_zFar						= 100.0f;	// meters
int2						g_vrSwapChainSize			= {2664, 1586};
bool						g_drawUI					= true;
int							g_triangleCounter			= 0;
int							g_perfHudMode				= int(ovrPerfHud_Off);

// Rendering
bool						g_vsync						= false;
int							g_repeatRenderingCount		= 1;
int 						g_msaaSampleCount			= 1;
bool						g_enableSSAO				= true;
bool						g_temporalAA				= true;
bool						g_computeMSAA				= true;
bool						g_suppressTemporalAA		= false;
int							g_sceneIndex				= 0;
std::string					g_sceneName					= "sponza.json";

// Common multi-projection data
Nv::VR::Data				g_projectionData			= {};
Nv::VR::Data				g_projectionDataPrev		= {};
Nv::VR::Data				g_projectionDataVR[2]		= {};
Nv::VR::Data				g_projectionDataVRPrev[2]	= {};
Nv::VR::Data				g_planarData				= {};
Nv::VR::Data				g_planarDataVR[2]			= {};

// MultiRes
bool						g_multiResEnabled			= false;
bool						g_drawMultiResSplits		= false;
bool						g_disableUpscale			= false;
Nv::VR::MultiRes::Configuration	g_multiResConfig		= Nv::VR::MultiRes::ConfigurationSet_OculusRift_CV1[0];
Nv::VR::MultiRes::Configuration	g_multiResConfigVR[2]	= {};

// PascalVR
bool						g_lensMatchedShadingEnabled	= false;
bool						g_drawLMSSplits				= false;
bool						g_disableUnwarp				= false;
Nv::VR::LensMatched::Configuration g_lensMatchedConfig	= Nv::VR::LensMatched::ConfigurationSet_OculusRift_CV1[0];
Nv::VR::LensMatched::Configuration g_lensMatchedConfigVR[2]	= {};
float						g_resolutionScale			= 1.0f;
bool						g_frustumCulling			= true;
bool						g_fakeVREnabled				= false;
bool						g_instancedStereoEnabled	= false;
bool						g_singlePassStereoEnabled	= false;
StereoMode					g_stereoMode				= StereoMode::NONE;
NvFeatureLevel				g_featureLevel				= NvFeatureLevel::GENERIC_DX11;

// Messages to show
const int					err_none						= 0;
const int					err_MRSPlusLMSNotSuppored		= 1;
const int					err_MRSPlusSPSNotSupported		= 2;
const int					err_StereoNotEnabled			= 3;
const int					err_InstancedPlanarOnly			= 4;

const char*					g_WindowTitle = "NVIDIA VRWorks Sample (DX11)";

// Constant buffers

struct CBFrame									// matches cbuffer CBFrame in shader-common.hlsli
{
	float4x4	m_matWorldToClip;
	float4x4	m_matWorldToClipR;
	float4x4	m_matWorldToClipPrev;
	float4x4	m_matWorldToClipPrevR;
	float4x4	m_matProjInv;
	float4x4	m_matWorldToViewNormal;
	float4x4	m_matWorldToUvzwShadow;
	float3x4	m_matWorldToUvzShadowNormal;	// actually float3x3, but constant buffer packing rules...
	
	float3		m_vecDirectionalLight;
	float		m_padding1;

	rgb			m_rgbDirectionalLight;
	float		m_padding2;

	float2		m_dimsShadowMap;
	float		m_normalOffsetShadow;
	float		m_shadowSharpening;

	float4		m_screenSize;

	float2		m_viewportOrigin;
	float2		m_randomOffset;

	float		m_temporalAAClampingFactor;
	float		m_temporalAANewFrameWeight;
	float		m_textureLodBias;
	float		m_padding3;
};

struct CBVRData
{
	Nv::VR::FastGSCBData	m_vrFastGSCBData;
	Nv::VR::FastGSCBData	m_vrFastGSCBDataR;
	Nv::VR::RemapCBData		m_vrRemapCBData;
	Nv::VR::RemapCBData		m_vrRemapCBDataR;
	Nv::VR::RemapCBData		m_vrRemapCBDataPrev;
	Nv::VR::RemapCBData		m_vrRemapCBDataPrevR;
};



enum GTS
{
	GTS_RenderScene,
	GTS_Count,
};



class VRWorksSample : public D3D11Window
{
public:
	typedef D3D11Window super;

	std::vector<Scene*>					m_scenes;
	comptr<ShaderFactory>				m_pShaderFactory;
	comptr<ShaderState>					m_pShaderState;
	bool								m_bCompilingShaders;
	std::thread*						m_pCompilingThread;
	bool								m_bLoadingScene;
	std::thread*						m_pSceneLoadingThread;
	Scene::LoadingStats					m_SceneLoadingStats;

	// Render targets
	RenderTarget						m_rtSceneMSAA;
	RenderTarget						m_rtMotionVectorsMSAA;
	RenderTarget						m_rtNormalsMSAA;
	RenderTarget						m_rtScene;
	RenderTarget						m_rtScenePrev;
	DepthStencilTarget					m_dstSceneMSAA;
	ShadowMap							m_shadowMap;
	RenderTarget						m_rtUpscaled;

	comptr<ID3D11DepthStencilState>		m_pDssDepthWriteNoDepthTest;
	CB<CBFrame>							m_cbFrame;
	CB<CBVRData>						m_cbVRData;
	Texture2D							m_GrayTexture;

	float4x4							m_matWorldToClipPrev;
	float4x4							m_matWorldToClipPrevR;
	FPSCamera							m_camera;
	Timer								m_timer;
	GPUProfiler							m_gpup;
	int									m_message_to_show; // ID of Error/Warning message to show

	// VR resources
	vr::IVRSystem*						m_pOpenVRSystem;
	vr::IVRCompositor*					m_pOpenVRCompositor;
	vr::TrackedDevicePose_t				m_poseOpenVR;
	ovrSession							m_oculusSession;
	ovrTextureSwapChain                 m_oculusTextureSwapChain;
	ovrFovPort							m_eyeFovOculusHMD[2];
	ovrVector3f							m_eyeOffsetsOculusHMD[2];
	ovrPosef							m_poseOculusHMD[2];
	std::vector<comptr<ID3D11RenderTargetView>> m_oculusSwapTextureRTVs;
	float4x4							m_matProjVR[2];


										VRWorksSample();

	bool								Init(HINSTANCE hInstance);
	void								InitUI();
	virtual void						Shutdown() override;
	virtual LRESULT						MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;
	virtual void						OnResize(int2_arg dimsNew) override;
	virtual void						OnRender() override;
	void								ShowErrorMessage();

	void								VerifyShaders();
	void								VerifyRenderTargetDims();
	void								ResetCamera();
	void								DrawObjects(ID3D11PixelShader * pPs, ID3D11PixelShader * pPsAlphaTest, bool bShadowMapPass);
	void								DrawObjectInstances(ID3D11PixelShader * pPs, ID3D11PixelShader * pPsAlphaTest, Scene* pScene, bool bShadowMapPass);
	void								CalcEyeMatrices(bool vrActive, int eye, float4x4* ref_worldToClip, point3* ref_cameraPos, affine3* ref_eyeToWorld);
	void								CalculateProjectionMatrices();
	void								RenderScene();
	void								DrawRepeatedScene();
	void								RenderShadowMap();
	void								FrustumCull(const float4x4& matWorldToClip);
	void								FrustumCullStereo(const float4x4& matWorldToClipLeft, const float4x4& matWorldToClipRight);
	void								LoadSceneAsync();
	Scene*								LoadObject(const std::string& rootPath, Json::Value& node, concurrency::task_group& taskGroup);
	StereoMode							GetCurrentStereoMode();

	// HMD support
	bool								TryActivateVR();
	void								DeactivateVR();
	bool								IsVRActive() const { return m_oculusSession || m_pOpenVRSystem; }
	bool								IsVROrFakeVRActive() const { return m_oculusSession || m_pOpenVRSystem || g_fakeVREnabled; }
	bool								TryActivateOculusVR();
	void								DeactivateOculusVR();
	bool								TryActivateOpenVR();
	void								DeactivateOpenVR();

	//  VR common
	void								RecalcVRData();
	void								SetVRCBData(const Nv::VR::Data& data, const Nv::VR::Data& dataR, const Nv::VR::Data& dataPrev, const Nv::VR::Data& dataPrevR, const int2_arg dimsRt, CBVRData* ref_cbData);
	void								SetVRViewports(ID3D11DeviceContext* context, const Nv::VR::ProjectionViewports& viewports, float2 viewportOffset = float2());
	void								SetVRCBDataMono(CBVRData* ref_cbData);
	void								SetVRCBDataSingleEye(int eye, CBVRData* ref_cbData);
	void								SetVRCBDataStereo(CBVRData* ref_cbData);
	void								SetVRViewportsMono(float2 viewportOffset = float2());
	void								SetVRViewportsSingleEye(int eye, float2 viewportOffset = float2());
	void								SetVRViewportsStereo(float2 viewportOffset = float2());
	void								FlattenImage();
	void								PresentToHMD(RenderTarget* pRT);

	void								DrawSplits(const Nv::VR::Data& data, RenderTarget* pRt, int Width, int Height);
	void								DrawSplits();

	// Lens-Matched shading
	void								EnableLensMatchedShading(const Nv::VR::LensMatched::Configuration* config);
	void								EnableLensMatchedShadingWithSinglePassStereo(const Nv::VR::LensMatched::Configuration* leftConfig, const Nv::VR::LensMatched::Configuration* rightConfig);
	void								DisableLensMatchedShading();
	void								DrawSafeZone();

};



VRWorksSample::VRWorksSample()
:	m_oculusSession(nullptr),
	m_pOpenVRSystem(nullptr),
	m_pOpenVRCompositor(nullptr),
	m_bCompilingShaders(false),
	m_pCompilingThread(nullptr),
	m_bLoadingScene(false),
	m_pSceneLoadingThread(nullptr)
{
	// Disable framework's automatic depth buffer, since we'll create our own
	m_hasDepthBuffer = false;

	memset(&m_matWorldToClipPrev, 0, sizeof(float4x4));
	memset(&m_matWorldToClipPrevR, 0, sizeof(float4x4));
}


void VRWorksSample::LoadSceneAsync()
{
	for (auto pScene : m_scenes)
		delete pScene;

	m_scenes.clear();

	m_shadowMap.Invalidate();

	std::string rootPath = "common/media/";
	std::string scenePath;

	for (int depth = 0; depth < 5; depth++)
	{
		scenePath = rootPath + g_sceneName;

		if (PathFileExistsA(scenePath.c_str()))
			break;

		rootPath = "../" + rootPath;
	}

	m_bLoadingScene = true;
	memset(&m_SceneLoadingStats, 0, sizeof(Scene::LoadingStats));

	m_pSceneLoadingThread = new std::thread([this, rootPath, scenePath]() 
	{
		std::ifstream sceneFile(scenePath);

		Json::Reader reader;
		Json::Value root;
		bool parsingSuccessful = reader.parse(sceneFile, root, false);
		if (!parsingSuccessful)
		{
			ERR("Couldn't load the scene description (json) file");
			return;
		}

		concurrency::task_group taskGroup;
		for (UINT index = 0; index < root.size(); index++)
		{
			m_scenes.push_back(LoadObject(rootPath, root[index], taskGroup));
		}
		taskGroup.wait();

		m_bLoadingScene = false;
	});
}

Scene* VRWorksSample::LoadObject(const std::string& rootPath, Json::Value& node, concurrency::task_group& taskGroup)
{
	InterlockedIncrement(&m_SceneLoadingStats.ObjectsTotal);

	Scene* object = new Scene();

	Json::Value& instances = node["instances"];
	for (UINT index = 0; index < instances.size(); index++)
	{
		Json::Value& instanceNode = instances[index];
		float4x4 matrix;
		for (UINT component = 0; component < 16; component++)
			matrix.m_data[component] = float(instanceNode[component].asDouble());
		object->AddInstance(matrix);
	}

	std::string fileName = rootPath + node["file"].asString();

	taskGroup.run([this, &taskGroup, fileName, object]()
	{
		if (FAILED(object->Load(fileName.c_str())))
		{
			ERR("Couldn't load scene file %s", fileName.c_str());
			return;
		}

		object->UpdateBounds();

		if (FAILED(object->InitResources(m_pDevice, m_SceneLoadingStats, taskGroup)))
		{
			ERR("Couldn't load the textures for scene file %s", fileName.c_str());
			return;
		}

		InterlockedIncrement(&m_SceneLoadingStats.ObjectsLoaded);
	});

	return object;
}

StereoMode VRWorksSample::GetCurrentStereoMode()
{
	StereoMode output = StereoMode::NONE;

	if (g_instancedStereoEnabled)
		output = StereoMode::INSTANCED;

	if (g_singlePassStereoEnabled)
		output = StereoMode::SINGLE_PASS;

	return output;
}

static void TW_CALL SetSceneCB(const void *value, void *userData)
{
	int valueInt = *(const int*)value;

	switch (valueInt)
	{
	case 0: 
		g_sceneName = "sponza.json";
		g_sceneIndex = 0;
		break;
	case 1:
		g_sceneName = "san-miguel.json";
		g_sceneIndex = 1;
		break;
	}

	VRWorksSample* sample = (VRWorksSample*)userData;
	sample->LoadSceneAsync();
	sample->ResetCamera();
}

static void TW_CALL GetSceneCB(void *value, void *)
{
	*(int*)value = g_sceneIndex;
}

bool VRWorksSample::Init(HINSTANCE hInstance)
{
	super::Init("VRWorksSampleDX11", g_WindowTitle, hInstance);

	NvAPI_Initialize();

	// Register the D3D device with NVAPI in order to safely call various NvAPI_D3D11_Create... functions 
	// from a separate shader compiling thread.
	NvAPI_D3D_RegisterDevice(m_pDevice);

	bool bSupported = false;
	// There is no way to explicitly check for Maxwell FastGS support except by trying to create such FastGS.
	// Test for FP16 atomics instead (which were introduced in the same GPU architecture as FastGS).
	if (NvAPI_D3D11_IsNvShaderExtnOpCodeSupported(m_pDevice, NV_EXTN_OP_FP16_ATOMIC, &bSupported) == NVAPI_OK && bSupported)
	{
		NV_QUERY_MODIFIED_W_SUPPORT_PARAMS modifiedWParams = { 0 };
		modifiedWParams.version = NV_QUERY_MODIFIED_W_SUPPORT_PARAMS_VER;
		NV_QUERY_SINGLE_PASS_STEREO_SUPPORT_PARAMS stereoParams = { 0 };
		stereoParams.version = NV_QUERY_SINGLE_PASS_STEREO_SUPPORT_PARAMS_VER;

		if (NvAPI_D3D_QueryModifiedWSupport(m_pDevice, &modifiedWParams) == NVAPI_OK && modifiedWParams.bModifiedWSupported &&
			NvAPI_D3D_QuerySinglePassStereoSupport(m_pDevice, &stereoParams) == NVAPI_OK && stereoParams.bSinglePassStereoSupported)
			g_featureLevel = NvFeatureLevel::PASCAL_GPU;
		else
		{
			g_featureLevel = NvFeatureLevel::MAXWELL_GPU;

			MessageBox(m_hWnd, "The GPU or the installed driver doesn't support the Pascal VR hardware features (Lens Matched Shading and Single Pass Stereo). "
				"Sample modes using these features will use software emulation with regular geometry shaders.", g_WindowTitle, MB_ICONEXCLAMATION);
		}
	}
	else
	{
		g_featureLevel = NvFeatureLevel::GENERIC_DX11;

		MessageBox(m_hWnd, "The GPU or the installed driver doesn't support either Maxwell or Pascal VR hardware features (Multi-Res Shading, Lens Matched Shading, and Single Pass Stereo). "
			"Sample modes using these features will use software emulation with regular geometry shaders.", g_WindowTitle, MB_ICONEXCLAMATION);
	}

	{
		std::string rootPath = "shaders/";

		for (int depth = 0; depth < 5; depth++)
		{
			// Find any shader that is in the same folder with others
			std::string shaderPath = rootPath + "shader-common.hlsli";

			if (PathFileExistsA(shaderPath.c_str()))
				break;

			rootPath = "../" + rootPath;
		}

		m_pShaderFactory = new ShaderFactory(m_pDevice, rootPath);
	}
	
	// Load shaders for the initial frame and to detect GPU features
	VerifyShaders();

	// Init shadow map
	m_shadowMap.Init(m_pDevice, makeint2(4096));

	// Start loading the scene
	LoadSceneAsync();

	// Init constant buffers
	m_cbFrame.Init(m_pDevice);
	m_cbVRData.Init(m_pDevice);

	// Init GPU profiler
	m_gpup.Init(m_pDevice, GTS_Count);

	// Init the camera
	m_camera.m_moveSpeed = 3.0f;
	m_camera.m_mbuttonActivate = MBUTTON_Left;
	ResetCamera();

	// Init AntTweakBar
	CHECK_ERR(TwInit(TW_DIRECT3D11, m_pDevice));

	// Init libovr
	ovrInitParams ovrParams = {};
	// Ignore the errors, success checked on Activate VR command
	ovr_Initialize(&ovrParams);

	D3D11_DEPTH_STENCIL_DESC dssDesc =
	{
		true,
		D3D11_DEPTH_WRITE_MASK_ALL,
		D3D11_COMPARISON_ALWAYS,
	};
	CHECK_D3D(m_pDevice->CreateDepthStencilState(&dssDesc, &m_pDssDepthWriteNoDepthTest));

	{
		unsigned int image = 0xff808080;

		D3D11_TEXTURE2D_DESC desc = { 0 };
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Width = 1;
		desc.Height = 1;
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		CHECK_D3D(m_pDevice->CreateTexture2D(&desc, nullptr, &m_GrayTexture.pResource));

		CHECK_D3D(m_pDevice->CreateShaderResourceView(m_GrayTexture.pResource, nullptr, &m_GrayTexture.pSRV));

		m_pCtx->UpdateSubresource(m_GrayTexture.pResource, 0, nullptr, &image, 4, 0);
	}

	InitUI();

	return true;
}

void VRWorksSample::VerifyShaders()
{
	if (m_bCompilingShaders)
		return;

	if (m_pCompilingThread && !m_bCompilingShaders)
	{
		m_pCompilingThread->join();
		delete m_pCompilingThread;
		m_pCompilingThread = nullptr;
	}

	ShaderState::Effect effect;
	effect.value = 0;

	effect.projection = 
		g_multiResEnabled ? uint(Nv::VR::Projection::MULTI_RES) : 
		g_lensMatchedShadingEnabled ? uint(Nv::VR::Projection::LENS_MATCHED) : 
		uint(Nv::VR::Projection::PLANAR);

	effect.stereoMode = uint(g_stereoMode);
	effect.featureLevel = uint(g_featureLevel);
	effect.msaaSampleCount = g_msaaSampleCount;
	effect.temporalAA = g_temporalAA;
	effect.computeMSAA = g_computeMSAA;


	if (!m_pShaderState || effect.value != m_pShaderState->m_Effect.value)
	{
		m_pShaderState.release();
		m_bCompilingShaders = true;

		m_pCompilingThread = new std::thread([this, effect]() 
		{ 
			m_pShaderState = new ShaderState(m_pShaderFactory, effect); 
			m_bCompilingShaders = false; 
		});
	}
}

static const Nv::VR::MultiRes::Configuration* MultiResPresets[] = {
	nullptr,
	&Nv::VR::MultiRes::Configuration_Balanced,
	&Nv::VR::MultiRes::Configuration_Aggressive,
	&Nv::VR::MultiRes::ConfigurationSet_OculusRift_CV1[0],
	&Nv::VR::MultiRes::ConfigurationSet_OculusRift_CV1[1],
	&Nv::VR::MultiRes::ConfigurationSet_OculusRift_CV1[2],
	&Nv::VR::MultiRes::ConfigurationSet_HTC_Vive[0],
	&Nv::VR::MultiRes::ConfigurationSet_HTC_Vive[1],
	&Nv::VR::MultiRes::ConfigurationSet_HTC_Vive[2],
};

static const Nv::VR::LensMatched::Configuration* LensMatchedPresets[] = {
	nullptr,
	&Nv::VR::LensMatched::ConfigurationSet_OculusRift_CV1[0],
	&Nv::VR::LensMatched::ConfigurationSet_OculusRift_CV1[1],
	&Nv::VR::LensMatched::ConfigurationSet_OculusRift_CV1[2],
	&Nv::VR::LensMatched::ConfigurationSet_HTC_Vive[0],
	&Nv::VR::LensMatched::ConfigurationSet_HTC_Vive[1],
	&Nv::VR::LensMatched::ConfigurationSet_HTC_Vive[2],
};

void VRWorksSample::InitUI()
{
	// Automatically use the biggest font size
	TwDefine("GLOBAL fontsize=3 fontresizable=false mouserepeat=false");

	#pragma region FPS

	// Create bar for FPS display
	TwBar * pTwBarFPS = TwNewBar("FPS");
	TwDefine("FPS position='10 10' size='320 130' valueswidth=80 refresh=0.5");
	TwAddVarCB(
			pTwBarFPS, "FPS", TW_TYPE_FLOAT,
			nullptr,
			[](void * value, void * timer) { 
				*(float *)value = 1.0f / float(((Timer*)timer)->m_smoothstep);
			},
			&m_timer,
			"precision=1");
	TwAddVarCB(
			pTwBarFPS, "CPU frame time (ms)", TW_TYPE_FLOAT,
			nullptr,
			[](void * value, void * timer) { 
				*(float *)value = 1000.0f * float(((Timer*)timer)->m_smoothstep);
			},
			&m_timer,
			"precision=2");
	TwAddVarCB(
			pTwBarFPS, "GPU render time (ms)", TW_TYPE_FLOAT,
			nullptr,
			[](void * value, void * gpup) {
				GPUProfiler * pGpup = (GPUProfiler *)gpup;
				*(float *)value = float(pGpup->m_msAvg[GTS_RenderScene]);
			},
			&m_gpup,
			"precision=2");
	TwAddVarCB(
			pTwBarFPS, "Triangle Count", TW_TYPE_INT32,
			nullptr,
			[](void * value, void * gpup) {
				(gpup);
				*(int*)value = g_triangleCounter;
			},
			nullptr,
		"precision=2");
	
	TwAddVarCB(
		pTwBarFPS, "Rendered MPixels", TW_TYPE_FLOAT,
		nullptr,
		[](void * value, void * demo) {
			float area = 0;
			bool vr = ((VRWorksSample*)demo)->IsVROrFakeVRActive();

			if (g_multiResEnabled)
			{
				if (vr)
					area = CalculateRenderedArea(g_multiResConfigVR[0], g_projectionDataVR[0].Viewports) + CalculateRenderedArea(g_multiResConfigVR[1], g_projectionDataVR[1].Viewports);
				else
					area = CalculateRenderedArea(g_multiResConfig, g_projectionData.Viewports);
			}
			else if (g_lensMatchedShadingEnabled)
			{
				if (vr)
					area = CalculateRenderedArea(g_lensMatchedConfigVR[0], g_projectionDataVR[0].Viewports) + CalculateRenderedArea(g_lensMatchedConfigVR[1], g_projectionDataVR[1].Viewports);
				else
				{
					area = CalculateRenderedArea(g_lensMatchedConfig, g_projectionData.Viewports);
				}
			}
			else
			{
				int2 dims = ((VRWorksSample*)demo)->m_rtScene.m_dims;
				area = float(dims.x * dims.y);
			}

			*(float*)value = area * 1e-6f;
		}, this, "precision=2");

	#pragma endregion

	#pragma region Rendering

	// Create bar for rendering options
	TwBar* pTwBarRendering = TwNewBar("Rendering");
	TwDefine("Rendering position='10 150' size='320 210' valueswidth=80");

	TwAddButton(pTwBarRendering, "Reload Shaders", 
		[](void* demo) {
		auto pDemo = (VRWorksSample*)demo;
		pDemo->m_pShaderFactory->ClearCache();
		pDemo->m_pShaderState.release();
		}, this, nullptr);

	TwAddVarRW(pTwBarRendering, "VSync", TW_TYPE_BOOLCPP, &g_vsync, nullptr);
	
	{
		TwEnumVal msaaEV[] =
		{
			{ 1, "No MSAA" },
			{ 2, "2x MSAA" },
			{ 4, "4x MSAA" },
			{ 8, "8x MSAA" },
		};
		TwType msaaModes = TwDefineEnum("MSAA Mode", msaaEV, sizeof(msaaEV) / sizeof(msaaEV[0]));
		TwAddVarRW(pTwBarRendering, "MSAA Mode", msaaModes, &g_msaaSampleCount, nullptr);
	}

	TwAddVarRW(pTwBarRendering, "Repeat Rendering", TW_TYPE_INT32, &g_repeatRenderingCount, "min=1 max=1000");

	TwAddVarRW(pTwBarRendering, "Resolution Scale", TW_TYPE_FLOAT, &g_resolutionScale, "min=0.1 max=3 step=0.01 precision=2");

	TwAddVarRW(pTwBarRendering, "SSAO", TW_TYPE_BOOLCPP, &g_enableSSAO, "");

	TwAddVarRW(pTwBarRendering, "Temporal AA", TW_TYPE_BOOLCPP, &g_temporalAA, "");
	TwAddVarRW(pTwBarRendering, "Compute MSAA", TW_TYPE_BOOLCPP, &g_computeMSAA, "");

	TwAddVarRW(pTwBarRendering, "Frustum Culling", TW_TYPE_BOOLCPP, &g_frustumCulling, "");

	{   // Scene selection
		TwEnumVal sceneEV[] = {
			{ 0, "Sponza" },
			{ 1, "San Miguel" }
		};
		TwType sceneType = TwDefineEnum("Scene", sceneEV, sizeof(sceneEV) / sizeof(sceneEV[0]));
		TwAddVarCB(pTwBarRendering, "Scene", sceneType, SetSceneCB, GetSceneCB, this, "");
	}

	#pragma endregion

	#pragma region MultiRes

	// Create bar for multi-res options and readouts
	TwBar* pTwBarMultiRes = TwNewBar("Multi-Res");
	TwDefine("Multi-Res size='320 200' valueswidth=160"); // position is updated in OnResize

	if (g_featureLevel < NvFeatureLevel::MAXWELL_GPU)
		TwDefine("Multi-Res label='Multi-Res Shading (EMULATED)'");
	else
		TwDefine("Multi-Res label='Multi-Res Shading'");
	
	TwAddVarCB(pTwBarMultiRes, "Enable Multi-Res", TW_TYPE_BOOLCPP, 
		[](const void* value, void* demo) // Set callback
		{
			(demo);
			g_multiResEnabled = *(bool*)value;
			g_suppressTemporalAA = true;
		},
		[](void* value, void* demo) // get callback
		{
			(demo);
			*(bool*)value = g_multiResEnabled;
		}, 
		this, nullptr);

	TwAddVarRW(pTwBarMultiRes, "Disable Flattening", TW_TYPE_BOOLCPP, &g_disableUpscale, nullptr);
	
	TwAddVarRW(pTwBarMultiRes, "Draw Splits", TW_TYPE_BOOLCPP, &g_drawMultiResSplits, nullptr);

	{
		TwEnumVal presetEV[] = {
			{ 0, "---" },
			{ 1, "Generic Balanced" },
			{ 2, "Generic Aggressive" },
			{ 3, "Oculus Rift Conservative" },
			{ 4, "Oculus Rift Balanced" },
			{ 5, "Oculus Rift Aggressive" },
			{ 6, "HTC Vive Conservative" },
			{ 7, "HTC Vive Balanced" },
			{ 8, "HTC Vive Aggressive" }
		};
		TwType presetType = TwDefineEnum("MRS preset", presetEV, dim(presetEV));
		TwAddVarCB(pTwBarMultiRes, "Preset Selection", presetType, 
			[](const void* value, void*) // Set callback
			{
				int index = *((int*)value);
				if (index > 0 && index < dim(MultiResPresets))
					g_multiResConfig = *MultiResPresets[index];
			},
			[](void* value, void*) // get callback
			{
				*((int*)value) = 0;
			}, this, nullptr);
	}

	TwAddSeparator(pTwBarMultiRes, nullptr, nullptr);
	
	TwAddVarRW(pTwBarMultiRes, "Center Width", TW_TYPE_FLOAT, &g_multiResConfig.CenterWidth, "group='Manual Control' min=0.01 max=1.00 step=0.01 precision=2");
	TwAddVarRW(pTwBarMultiRes, "Center Height", TW_TYPE_FLOAT, &g_multiResConfig.CenterHeight, "group='Manual Control' min=0.01 max=1.00 step=0.01 precision=2");
	TwAddVarRW(pTwBarMultiRes, "Center X", TW_TYPE_FLOAT, &g_multiResConfig.CenterX, "group='Manual Control' min=0.00 max=1.00 step=0.01 precision=2");
	TwAddVarRW(pTwBarMultiRes, "Center Y", TW_TYPE_FLOAT, &g_multiResConfig.CenterY, "group='Manual Control' min=0.00 max=1.00 step=0.01 precision=2");

	TwAddSeparator(pTwBarMultiRes, nullptr, "group='Manual Control'");

	const char* resolutionDef = "group='Manual Control' min=0.05 max=2.0 step=0.01 precision=2";
	TwAddVarRW(pTwBarMultiRes, "DensityScaleX[0]", TW_TYPE_FLOAT, &g_multiResConfig.DensityScaleX[0], resolutionDef);
	TwAddVarRW(pTwBarMultiRes, "DensityScaleX[1]", TW_TYPE_FLOAT, &g_multiResConfig.DensityScaleX[1], resolutionDef);
	TwAddVarRW(pTwBarMultiRes, "DensityScaleX[2]", TW_TYPE_FLOAT, &g_multiResConfig.DensityScaleX[2], resolutionDef);
	TwAddVarRW(pTwBarMultiRes, "DensityScaleY[0]", TW_TYPE_FLOAT, &g_multiResConfig.DensityScaleY[0], resolutionDef);
	TwAddVarRW(pTwBarMultiRes, "DensityScaleY[1]", TW_TYPE_FLOAT, &g_multiResConfig.DensityScaleY[1], resolutionDef);
	TwAddVarRW(pTwBarMultiRes, "DensityScaleY[2]", TW_TYPE_FLOAT, &g_multiResConfig.DensityScaleY[2], resolutionDef);

	TwDefine("Multi-Res/'Manual Control' opened=false");

	#pragma endregion

	#pragma region Lens Matched Shading

	TwBar* pTwBarPascalVR = TwNewBar("LensMatchedShading");
	TwDefine("LensMatchedShading size='320 200' valueswidth=160"); // position is updated in OnResize

	if (g_featureLevel < NvFeatureLevel::PASCAL_GPU)
		TwDefine("LensMatchedShading label='Lens Matched Shading (EMULATED)'");
	else
		TwDefine("LensMatchedShading label='Lens Matched Shading'");
	
	TwAddVarRW(pTwBarPascalVR, "Enable Lens Matched Shading", TW_TYPE_BOOLCPP, &g_lensMatchedShadingEnabled, nullptr);
	
	TwAddVarRW(pTwBarPascalVR, "Disable Flattening", TW_TYPE_BOOLCPP, &g_disableUnwarp, nullptr);

	TwAddVarRW(pTwBarPascalVR, "Draw Splits", TW_TYPE_BOOLCPP, &g_drawLMSSplits, nullptr);
	
	{
		TwEnumVal presetEV[] = {
			{ 0, "---" },
			{ 1, "Oculus Rift Conservative" },
			{ 2, "Oculus Rift Balanced" },
			{ 3, "Oculus Rift Aggressive" },
			{ 4, "HTC Vive Conservative" },
			{ 5, "HTC Vive Balanced" },
			{ 6, "HTC Vive Aggressive" }
		};
		TwType presetType = TwDefineEnum("LMS preset", presetEV, dim(presetEV));
		TwAddVarCB(pTwBarPascalVR, "Preset Selection", presetType, 
			[](const void* value, void*) // Set callback
			{
				int index = *((int*)value);
				if (index > 0 && index < dim(LensMatchedPresets))
					g_lensMatchedConfig = *LensMatchedPresets[index];
			},
			[](void* value, void*) // get callback
			{
				*((int*)value) = 0;
			}, this, nullptr);
	}

	TwAddSeparator(pTwBarPascalVR, nullptr, nullptr);

	TwAddVarRW(pTwBarPascalVR, "Warp Left", TW_TYPE_FLOAT, &g_lensMatchedConfig.WarpLeft, "group='Manual Control' min=0.0 max=10 step=0.01 precision=2");
	TwAddVarRW(pTwBarPascalVR, "Warp Right", TW_TYPE_FLOAT, &g_lensMatchedConfig.WarpRight, "group='Manual Control' min=0.0 max=10 step=0.01 precision=2");
	TwAddVarRW(pTwBarPascalVR, "Warp Up", TW_TYPE_FLOAT, &g_lensMatchedConfig.WarpUp, "group='Manual Control' min=0.0 max=10 step=0.01 precision=2");
	TwAddVarRW(pTwBarPascalVR, "Warp Down", TW_TYPE_FLOAT, &g_lensMatchedConfig.WarpDown, "group='Manual Control' min=0.0 max=10 step=0.01 precision=2");

	TwAddVarCB(pTwBarPascalVR, "Warp All", TW_TYPE_FLOAT,
		[](const void* value, void* demo) // Set callback
		{
			(demo);
			g_lensMatchedConfig.WarpUp = g_lensMatchedConfig.WarpDown = g_lensMatchedConfig.WarpLeft = g_lensMatchedConfig.WarpRight = *(float*)value;
		},
		[](void* value, void* demo) // get callback
		{
			(demo);
			*(float*)value = g_lensMatchedConfig.WarpLeft;
		},
		this, "min=0.0 max=10 step=0.01 precision=2");

	TwAddVarRW(pTwBarPascalVR, "Size Left", TW_TYPE_FLOAT, &g_lensMatchedConfig.RelativeSizeLeft, "group='Manual Control' min=0.05 max=0.5 step=0.01 precision=2");
	TwAddVarRW(pTwBarPascalVR, "Size Right", TW_TYPE_FLOAT, &g_lensMatchedConfig.RelativeSizeRight, "group='Manual Control' min=0.05 max=0.5 step=0.01 precision=2");
	TwAddVarRW(pTwBarPascalVR, "Size Up", TW_TYPE_FLOAT, &g_lensMatchedConfig.RelativeSizeUp, "group='Manual Control' min=0.05 max=0.5 step=0.01 precision=2");
	TwAddVarRW(pTwBarPascalVR, "Size Down", TW_TYPE_FLOAT, &g_lensMatchedConfig.RelativeSizeDown, "group='Manual Control' min=0.05 max=0.5 step=0.01 precision=2");

	TwDefine("LensMatchedShading/'Manual Control' opened=false");

	#pragma endregion

	#pragma region VR

	// Create bar for VR headset activation
	TwBar * pTwBarVR = TwNewBar("VR and Stereo");
	TwDefine("'VR and Stereo' position='10 370' size='320 180' valueswidth=80");
	
	TwAddButton(
		pTwBarVR, "Activate VR",
		[](void* window)
		{
			g_fakeVREnabled = false;
			((VRWorksSample *)window)->TryActivateVR();
		}, this, nullptr);

	TwAddButton(
		pTwBarVR, "Activate Desktop Stereo",
		[](void* window)
		{
			(window);
			g_fakeVREnabled = true;
			((VRWorksSample *)window)->DeactivateVR();
		}, this, nullptr);

	TwAddButton(
		pTwBarVR, "Deactivate VR/Stereo",
		[](void* window)
		{
			g_fakeVREnabled = false;
			((VRWorksSample *)window)->DeactivateVR();
		}, this, nullptr);

	TwAddSeparator(pTwBarVR, nullptr, nullptr);

	TwAddVarCB(pTwBarVR, "Instanced Stereo", TW_TYPE_BOOLCPP,
		[](const void* value, void* demo) // Set callback
		{
			(demo);
			g_instancedStereoEnabled = *(bool*)value;
			g_singlePassStereoEnabled = false;
		},
		[](void* value, void* demo) // get callback
		{
			(demo);
			*(bool*)value = g_instancedStereoEnabled;
		},
		this, "");

	TwAddVarCB(pTwBarVR, "Single Pass Stereo", TW_TYPE_BOOLCPP,
		[](const void* value, void* demo) // Set callback
		{
			(demo);
			g_instancedStereoEnabled = false;
			g_singlePassStereoEnabled = *(bool*)value;
		},
		[](void* value, void* demo) // get callback
		{
			(demo);
			*(bool*)value = g_singlePassStereoEnabled;
		},
		this, "");

	if (g_featureLevel < NvFeatureLevel::PASCAL_GPU)
		TwDefine("'VR and Stereo'/'Single Pass Stereo' label='Single Pass Stereo (EMULATED)'");
	 
	TwAddSeparator(pTwBarVR, nullptr, nullptr);

	{
		TwEnumVal perfHudModeEV[] = {
			{ ovrPerfHud_Off,           "Off" },
			{ ovrPerfHud_LatencyTiming, "Latency timing" },
			{ ovrPerfHud_AppRenderTiming,  "Render timing" },
			{ ovrPerfHud_PerfSummary,  "Perf headroom" },
			{ ovrPerfHud_VersionInfo,   "Version info" },
		};
		TwType perfHudModeType = TwDefineEnum("perfHudMode", perfHudModeEV, sizeof(perfHudModeEV) / sizeof(perfHudModeEV[0]));
		TwAddVarRW(pTwBarVR, "Perf HUD mode", perfHudModeType, &g_perfHudMode, "keyIncr=h keyDecr=H");
	}

	#pragma endregion
}

void VRWorksSample::Shutdown()
{
	DeactivateVR();
	ovr_Shutdown();

	TwTerminate();

	for (auto pScene : m_scenes)
		delete pScene;
	m_scenes.clear();

	m_rtSceneMSAA.Reset();
	m_rtMotionVectorsMSAA.Reset();
	m_rtNormalsMSAA.Reset();
	m_rtScene.Reset();
	m_rtScenePrev.Reset();
	m_dstSceneMSAA.Reset();
	m_shadowMap.Reset();
	m_rtUpscaled.Reset();

	m_cbFrame.Reset();
	m_cbVRData.Reset();

	m_GrayTexture.Release();

	m_gpup.Reset();

	super::Shutdown();
}

void VRWorksSample::ShowErrorMessage()
{
	// Had to move showing messagebox to message loop instead of showing it stright from render loop
	// [to make sure AntTweakBar's handling of inputs is not falled into infinite loop]

	int message = m_message_to_show;
	m_message_to_show = err_none;
	switch (message)
	{
		case err_none: break;
		case err_MRSPlusLMSNotSuppored:
		{
			MessageBox(
				m_hWnd,
				"Only multi-res or Lens Matched Shading can be enabled at same time. :(\nDisabling mutires-res rendering.",
				g_WindowTitle, MB_OK);
			break;
		};
		case err_MRSPlusSPSNotSupported:
		{
			MessageBox(
				m_hWnd,
				"Multi-res is not compatible with Single Pass Stereo. Disabling Multi-res.",
				g_WindowTitle, MB_OK);
			break;
		};
		case err_StereoNotEnabled:
		{
			MessageBox(
				m_hWnd,
				"Please enable stereo before trying to change stereo rendering mode.",
				g_WindowTitle, MB_OK);
			break;
		};
		case err_InstancedPlanarOnly:
		{
			MessageBox(
				m_hWnd,
				"Instanced Stereo mode is incompatible with either Multi-Res Shading or Lens Matched Shading. Disabling both.",
				g_WindowTitle, MB_OK);
			break;
		};
	}
}

LRESULT VRWorksSample::MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// show any pending error messages
	if (m_message_to_show) ShowErrorMessage();

	if (!((!g_drawUI || m_bLoadingScene) && message >= WM_MOUSEFIRST && message <= WM_MOUSELAST))
	{
		// Let AntTweakBar try to process the message
		if (TwEventWin(hWnd, message, wParam, lParam))
			return 0;
	}

	// Let the camera try to process the message
	if (m_camera.HandleWindowsMessage(message, wParam, lParam))
		return 0;

	switch (message)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_HOME:
			ResetCamera();
			break;

		case VK_ESCAPE:
			Shutdown();
			break;

		case VK_TAB:
			g_drawUI = !g_drawUI;
			break;

		case 'R':
			if (m_oculusSession)
				ovr_RecenterTrackingOrigin(m_oculusSession);
			else if (m_pOpenVRSystem)
				m_pOpenVRSystem->ResetSeatedZeroPose();
			break;
		}
		return 0;

	default:
		return super::MsgProc(hWnd, message, wParam, lParam);
	}
}

void VRWorksSample::ResetCamera()
{
	m_camera.LookAt(
				makepoint3(-6.8f, 1.6f, 13.8f),
				makepoint3(-5.8f, 1.6f, 13.8f));
}

void VRWorksSample::OnResize(int2_arg dimsNew)
{
	super::OnResize(dimsNew);

	char buf[100];
	int x = dimsNew.x - 330;
	sprintf_s(buf, "Multi-Res position='%d 10'", x);
	TwDefine(buf);
	sprintf_s(buf, "LensMatchedShading position='%d 220'", x);
	TwDefine(buf);
}

void VRWorksSample::VerifyRenderTargetDims()
{
	int2 requiredSceneSize;
	int2 requiredUpscaledSize;

	if (IsVRActive())
	{
		requiredUpscaledSize = g_vrSwapChainSize;
	}
	else
	{
		requiredUpscaledSize = m_dims;
	}


	{
		Nv::VR::Float2 projectionSize;
		Nv::VR::Float2 scaledFlattenedSize = Nv::VR::Float2(requiredUpscaledSize.x * g_resolutionScale, requiredUpscaledSize.y * g_resolutionScale);

		if (IsVROrFakeVRActive())
			scaledFlattenedSize.x *= 0.5f;

		if (g_lensMatchedShadingEnabled)
		{
			projectionSize = CalculateProjectionSize(scaledFlattenedSize, g_lensMatchedConfig);
		}
		else if (g_multiResEnabled)
		{
			projectionSize = CalculateProjectionSize(scaledFlattenedSize, g_multiResConfig);
		}
		else
		{
			projectionSize = scaledFlattenedSize;
		}

		if (IsVROrFakeVRActive())
			projectionSize.x *= 2.f;

		requiredSceneSize = makeint2(int(projectionSize.x), int(projectionSize.y));
	}

	int samples = g_msaaSampleCount;
	DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;
	DXGI_FORMAT rtSceneFormat = g_temporalAA ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	RTFLAG rtSceneFlag = g_temporalAA ? RTFLAG_EnableUAV : RTFLAG_Default;

	if (all(requiredSceneSize == m_rtSceneMSAA.m_dims) &&
		all(requiredUpscaledSize == m_rtUpscaled.m_dims) &&
		depthFormat == m_dstSceneMSAA.m_formatDsv &&
		rtSceneFormat == m_rtScene.m_format && 
		samples == m_rtSceneMSAA.m_sampleCount)
	{
		// All parameters match, no need to change anything
		return;
	}
	
	int2 dimsRta = { requiredUpscaledSize.x / 2, requiredUpscaledSize.y };
	
	m_rtSceneMSAA.Reset();
	m_rtMotionVectorsMSAA.Reset();
	m_rtNormalsMSAA.Reset();
	m_rtScene.Reset();
	m_rtScenePrev.Reset();
	m_dstSceneMSAA.Reset();
	m_rtUpscaled.Reset();
	
	m_rtSceneMSAA.Init(m_pDevice, requiredSceneSize, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, samples);
	m_rtMotionVectorsMSAA.Init(m_pDevice, requiredSceneSize, DXGI_FORMAT_R16G16_FLOAT, samples);
	m_rtNormalsMSAA.Init(m_pDevice, requiredSceneSize, DXGI_FORMAT_R8G8B8A8_SNORM, samples);
	m_rtScene.Init(m_pDevice, requiredSceneSize, rtSceneFormat, 1, rtSceneFlag);
	m_rtScenePrev.Init(m_pDevice, requiredSceneSize, rtSceneFormat, 1, rtSceneFlag);
	m_dstSceneMSAA.Init(m_pDevice, requiredSceneSize, depthFormat, samples);

	m_rtUpscaled.Init(m_pDevice, requiredUpscaledSize, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);

	g_suppressTemporalAA = true;
}

void VRWorksSample::OnRender()
{
	if (!IsVROrFakeVRActive() && (g_singlePassStereoEnabled || g_instancedStereoEnabled))
	{
		g_singlePassStereoEnabled = false;
		g_instancedStereoEnabled = false;
		m_message_to_show = err_StereoNotEnabled;
	}
	if (g_lensMatchedShadingEnabled && g_multiResEnabled)
	{
		g_multiResEnabled = false;
		m_message_to_show = err_MRSPlusLMSNotSuppored;
	}
	if (g_singlePassStereoEnabled && g_multiResEnabled)
	{
		g_multiResEnabled = false;
		m_message_to_show = err_MRSPlusSPSNotSupported;
	}

	if (g_instancedStereoEnabled && (g_multiResEnabled || g_lensMatchedShadingEnabled))
	{
		g_multiResEnabled = false;
		g_lensMatchedShadingEnabled = false;
		m_message_to_show = err_InstancedPlanarOnly;
	}

	g_stereoMode = GetCurrentStereoMode();

	VerifyShaders();

	m_timer.OnFrameStart();
	m_camera.Update(float(m_timer.m_timestep));

	if (IsVRActive())
	{
		// Force camera pitch to zero, so pitch is only from HMD orientation
		m_camera.m_pitch = 0.0f;
		m_camera.UpdateOrientation();

		// Retrieve head-tracking information from the VR API
		if (m_oculusSession)
		{
			ovr_GetEyePoses(m_oculusSession, m_timer.m_frameCount, true, m_eyeOffsetsOculusHMD, m_poseOculusHMD, nullptr);
			ovr_SetInt(m_oculusSession, OVR_PERF_HUD_MODE, int(g_perfHudMode));
		}
		else if (m_pOpenVRSystem)
		{
			CHECK_OPENVR_WARN(m_pOpenVRCompositor->WaitGetPoses(&m_poseOpenVR, 1, nullptr, 0));
		}
	}
#ifdef USE_GPU_TIMER
	m_gpup.OnFrameStart(m_pCtx);
#endif
	m_pCtx->ClearState();

	RenderTarget * pRtFinal = &m_rtScene;

	if (m_pSceneLoadingThread && !m_bLoadingScene)
	{
		m_pSceneLoadingThread->join();
		delete m_pSceneLoadingThread;
		m_pSceneLoadingThread = nullptr;

		for (auto scene : m_scenes)
			scene->FinalizeInit(m_pCtx);
	}

	if (m_pShaderState && !m_bLoadingScene)
	{
		RenderShadowMap();

		VerifyRenderTargetDims();

		RecalcVRData();

		RenderScene();

		DrawSplits();

		if (g_multiResEnabled && !g_disableUpscale || g_lensMatchedShadingEnabled && !g_disableUnwarp)
		{
			FlattenImage();
			pRtFinal = &m_rtUpscaled;
		}
	}

#ifdef USE_GPU_TIMER
	m_gpup.Mark(m_pCtx, GTS_RenderScene);
#endif
	
	if (m_pShaderState)
	{
		// Only present to the HMD if the shaders finished compiling. Otherwise the presented image is old/doesn't match the view.
		PresentToHMD(pRtFinal);
	}

	if (m_bLoadingScene)
	{
		m_pCtx->ClearRenderTargetView(m_pRtvRaw, makergba(0.f));
		
		BindRawBackBuffer(m_pCtx);

		const char* text = "Loading the scene, please wait...";
		int width = 0, height = 0;
		TwMeasureTextLine(text, &width, &height);
		TwBeginText((m_dims.x - width) / 2, (m_dims.y - height) / 2, 0, 0);
		TwAddTextLine(text, ~0u, 0);
		char buf[100];
		sprintf_s(buf, "Objects: %d/%d, Textures: %d/%d", 
			m_SceneLoadingStats.ObjectsLoaded, m_SceneLoadingStats.ObjectsTotal, 
			m_SceneLoadingStats.TexturesLoaded, m_SceneLoadingStats.TexturesTotal);
		TwAddTextLine(buf, ~0u, 0);
		TwEndText();
	}
	else
	{
		{
			PERF_SECTION(Blit);

			// Blit the frame to the window 
			BindSRGBBackBuffer(m_pCtx);

			BlitFullscreen(m_pCtx, pRtFinal->m_pSrv);
		}

		if (g_drawUI)
		{
			PERF_SECTION(AntTweakBar);

			// Draw AntTweakBar UI on window (not on VR headset)
			BindRawBackBuffer(m_pCtx);
			CHECK_WARN(TwDraw());
		}
	}

#ifdef USE_GPU_TIMER
	m_gpup.OnFrameEnd(m_pCtx);
#endif

	// Present to window - no vsync in VR mode; assume the VR API will take care of that
	bool vsync = (g_vsync || m_bLoadingScene) && !IsVRActive();
	CHECK_D3D(m_pSwapChain->Present(vsync ? 1 : 0, 0));
}

void VRWorksSample::PresentToHMD(RenderTarget* pRT)
{

	bool vrDisplayLost = false;
	if (m_oculusSession)
	{
		// Blit the frame to the Oculus swap texture set (would have rendered directly to it,
		// but then it seems you can't blit from it to the back buffer - we just get black).
		// this was the case in the 0.8 SDK, but not sure if it's still true in 1.3
		int currentIndex = -1;
		ovr_GetTextureSwapChainCurrentIndex(m_oculusSession, m_oculusTextureSwapChain, &currentIndex);
		ID3D11RenderTargetView* pRTV = m_oculusSwapTextureRTVs[currentIndex];

		m_pCtx->OMSetRenderTargets(1, &pRTV, nullptr);

		// make sure that pRtFinal is transferred 1:1 onto the swap chain
		SetViewport(m_pCtx, makefloat2(0, 0), makefloat2(float(g_vrSwapChainSize.x), float(g_vrSwapChainSize.y)));
		D3D11_RECT scissor = { 0, 0, g_vrSwapChainSize.x, g_vrSwapChainSize.y };
		m_pCtx->RSSetScissorRects(1, &scissor);

		BlitFullscreen(m_pCtx, pRT->m_pSrv);

		// Commit changes to the swap chain
		ovr_CommitTextureSwapChain(m_oculusSession, m_oculusTextureSwapChain);

		// Submit the frame to the Oculus runtime
		ovrResult result;

		ovrLayerEyeFov layerMain = {};
		layerMain.Header.Type = ovrLayerType_EyeFov;
		layerMain.Header.Flags = ovrLayerFlag_HighQuality;
		layerMain.ColorTexture[0] = m_oculusTextureSwapChain;
		layerMain.Viewport[ovrEye_Left].Size.w = g_vrSwapChainSize.x / 2;
		layerMain.Viewport[ovrEye_Left].Size.h = g_vrSwapChainSize.y;
		layerMain.Viewport[ovrEye_Right].Pos.x = g_vrSwapChainSize.x / 2;
		layerMain.Viewport[ovrEye_Right].Size.w = g_vrSwapChainSize.x / 2;
		layerMain.Viewport[ovrEye_Right].Size.h = g_vrSwapChainSize.y;
		layerMain.Fov[ovrEye_Left] = m_eyeFovOculusHMD[ovrEye_Left];
		layerMain.Fov[ovrEye_Right] = m_eyeFovOculusHMD[ovrEye_Right];
		layerMain.RenderPose[ovrEye_Left] = m_poseOculusHMD[ovrEye_Left];
		layerMain.RenderPose[ovrEye_Right] = m_poseOculusHMD[ovrEye_Right];

		ovrLayerHeader * layerList[] = { (ovrLayerHeader*)&layerMain };
		result = ovr_SubmitFrame(m_oculusSession, m_timer.m_frameCount, nullptr, layerList, dim(layerList));

		if (result == ovrError_DisplayLost)
		{
			// Display was powered off, or something. Set flag to turn off VR mode next frame.
			vrDisplayLost = true;
		}
		else if (OVR_FAILURE(result))
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo(&errorInfo);
			WARN("ovr_SubmitFrame failed with error code: %d\nError message: %s", result, errorInfo.ErrorString);
		}
	}
	else if (m_pOpenVRSystem)
	{
		RenderTarget* target = pRT;

		if (pRT != &m_rtUpscaled && pRT->m_format == DXGI_FORMAT_R16G16B16A16_FLOAT)
		{
			// If the final image is in RGBA16F format, copy it into m_rtUpscaled to avoid a DX runtime error when OpenVR calls CopySubresourceRegion.
			m_rtUpscaled.Bind(m_pCtx);
			BlitFullscreen(m_pCtx, pRT->m_pSrv);
			target = &m_rtUpscaled;
		}

		// Submit the frame to the OpenVR runtime
		vr::Texture_t tex = { target->m_pTex, vr::API_DirectX, vr::ColorSpace_Gamma };
		vr::VRTextureBounds_t bounds = { 0.0f, 0.0f, 0.5f, 1.0f };
		CHECK_OPENVR_WARN(m_pOpenVRCompositor->Submit(vr::Eye_Left, &tex, &bounds));
		bounds = { 0.5f, 0.0f, 1.0f, 1.0f };
		CHECK_OPENVR_WARN(m_pOpenVRCompositor->Submit(vr::Eye_Right, &tex, &bounds));
	}

	// Turn off VR mode if we lost the headset. 
	if (vrDisplayLost)
		DeactivateVR();
}

void VRWorksSample::FrustumCull(const float4x4& matWorldToClip)
{
	Frustum frustum(matWorldToClip);

	int maxObjects = (int)m_scenes.size();
	for (int i = 0; i < maxObjects; ++i)
	{
		Scene* pScene = m_scenes[i];
		pScene->FrustumCull(frustum, m_pCtx, g_frustumCulling);
	}
}

void VRWorksSample::FrustumCullStereo(const float4x4& matWorldToClipLeft, const float4x4& matWorldToClipRight)
{
	Frustum frustumLeft(matWorldToClipLeft);
	Frustum frustumRight(matWorldToClipRight);
	Frustum frustum = frustumLeft;
	frustum.planes[Frustum::RIGHT_PLANE] = frustumRight.planes[Frustum::RIGHT_PLANE];
	
	int maxObjects = (int)m_scenes.size();
	for (int i = 0; i < maxObjects; ++i)
	{
		Scene* pScene = m_scenes[i];
		pScene->FrustumCull(frustum, m_pCtx, g_frustumCulling);
	}
}

void VRWorksSample::DrawObjects(ID3D11PixelShader * pPs, ID3D11PixelShader * pPsAlphaTest, bool bShadowMapPass)
{
	int maxObjects = (int)m_scenes.size();
	for (int i = 0; i < maxObjects; ++i)
	{
		Scene* pScene = m_scenes[i];
		DrawObjectInstances(pPs, pPsAlphaTest, pScene, bShadowMapPass);
	}
}

void VRWorksSample::DrawObjectInstances(ID3D11PixelShader * pPs, ID3D11PixelShader * pPsAlphaTest, Scene* pScene, bool bShadowMapPass)
{
	m_pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pCtx->IASetInputLayout(m_pShaderState->m_pInputLayout);

	{
		ID3D11Buffer *pIB, *pVB;
		UINT stride, offset;
		pIB = pScene->GetIndexBuffer(0, offset);
		pVB = pScene->GetVertexBuffer(0, offset);
		stride = sizeof(Vertex);
		offset = 0;
		m_pCtx->IASetIndexBuffer(pIB, DXGI_FORMAT_R32_UINT, 0);
		m_pCtx->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);
	}

	UINT numMeshes = pScene->GetMeshesNum();
	const Material* lastMaterial = NULL;

	if(pScene->IsSingleInstanceBuffer())
	{
		ID3D11ShaderResourceView* pSRV = pScene->GetInstanceBuffer(0);
		m_pCtx->VSSetShaderResources(0, 1, &pSRV);
	}

	for (UINT i = 0; i < numMeshes; ++i)
	{
		const Material* material = pScene->GetMaterial(i);

		if (material == NULL)
			continue;

		UINT numInstances = pScene->GetCulledInstancesNum(i);

		if (numInstances == 0)
			continue;

		if(!pScene->IsSingleInstanceBuffer())
		{
			ID3D11ShaderResourceView* pSRV = pScene->GetInstanceBuffer(i);
			m_pCtx->VSSetShaderResources(0, 1, &pSRV);
		}

		if (material != lastMaterial)
		{
			ID3D11ShaderResourceView* pSRVs[5];
			if (material->m_DiffuseTexture)
				pSRVs[TEX_DIFFUSE] = material->m_DiffuseTexture->pSRV;
			else
				pSRVs[TEX_DIFFUSE] = m_GrayTexture.pSRV;

			if (bShadowMapPass)
			{
				pSRVs[TEX_NORMALS] = NULL;
				pSRVs[TEX_SPECULAR] = NULL;
				pSRVs[TEX_EMISSIVE] = NULL;
				pSRVs[TEX_SHADOW] = NULL;
			}
			else
			{
				pSRVs[TEX_NORMALS] = material->m_NormalsTexture ? material->m_NormalsTexture->pSRV : NULL;
				pSRVs[TEX_SPECULAR] = material->m_SpecularTexture ? material->m_SpecularTexture->pSRV : NULL;
				pSRVs[TEX_EMISSIVE] = material->m_EmissiveTexture ? material->m_EmissiveTexture->pSRV : NULL;
				pSRVs[TEX_SHADOW] = m_shadowMap.m_dst.m_pSrvDepth;
			}

			m_pCtx->PSSetShaderResources(0, dim(pSRVs), pSRVs);

			if (lastMaterial == NULL || material->m_AlphaTested != lastMaterial->m_AlphaTested)
			{
				if (material->m_AlphaTested)
				{
					m_pCtx->RSSetState(m_pRsDoubleSided);
					m_pCtx->PSSetShader(pPsAlphaTest, 0, 0);
					m_pCtx->OMSetBlendState(m_pBsAlphaToCoverage, NULL, ~0U);
				}
				else
				{
					m_pCtx->RSSetState(m_pRsDefault);
					m_pCtx->PSSetShader(pPs, 0, 0);
					m_pCtx->OMSetBlendState(NULL, NULL, ~0U);
				}
			}

			lastMaterial = material;
		}

		UINT indexOffset, vertexOffset;
		pScene->GetIndexBuffer(i, indexOffset);
		pScene->GetVertexBuffer(i, vertexOffset);
		
		if (g_instancedStereoEnabled)
		{
			numInstances *= 2;
		}
		int indexCount = pScene->GetMeshIndicesNum(i);
		m_pCtx->DrawIndexedInstanced(indexCount, numInstances, indexOffset, vertexOffset, 0);

		int triangleCount = indexCount * numInstances / 3;
		g_triangleCounter += triangleCount;
	}

	m_pCtx->RSSetState(m_pRsDefault);
	m_pCtx->PSSetShader(NULL, 0, 0);
	m_pCtx->OMSetBlendState(NULL, NULL, ~0U);
}

void VRWorksSample::CalcEyeMatrices(bool vrActive, int eye, float4x4* ref_worldToClip, point3* ref_cameraPos, affine3* ref_eyeToWorld)
{
	if (vrActive)
	{
		// Figure out the camera pose for this eye, from the VR tracking system
		affine3 eyeToCamera = affine3::identity();
		if (m_oculusSession)
		{
			ovrPosef hmdPose = m_poseOculusHMD[eye];
			// negate Z for RH to LH coordinate space transform, also negate x and y in quaternion to account for LH vs RH rotation
			hmdPose.Position.z = -hmdPose.Position.z;

			quat hmdOrientation =
			{
				hmdPose.Orientation.w,		// Note, different layout from Oculus quaternion!
				-hmdPose.Orientation.x,
				-hmdPose.Orientation.y,
				hmdPose.Orientation.z
			};
			eyeToCamera = makeaffine3(hmdOrientation, makefloat3(&hmdPose.Position.x));
		}
		else if (m_pOpenVRSystem)
		{
			vr::HmdMatrix34_t eyeToHeadOpenVR = m_pOpenVRSystem->GetEyeToHeadTransform(vr::EVREye(eye));
			affine3 eyeToHMD =
			{	// transposed from column-vector to row-vector convention
				// negate Z to transform from RH to LH coordinates
				eyeToHeadOpenVR.m[0][0], eyeToHeadOpenVR.m[1][0], -eyeToHeadOpenVR.m[2][0],
				eyeToHeadOpenVR.m[0][1], eyeToHeadOpenVR.m[1][1], -eyeToHeadOpenVR.m[2][1],
				eyeToHeadOpenVR.m[0][2], eyeToHeadOpenVR.m[1][2], -eyeToHeadOpenVR.m[2][2],
				eyeToHeadOpenVR.m[0][3], eyeToHeadOpenVR.m[1][3], -eyeToHeadOpenVR.m[2][3],
			};

			vr::HmdMatrix34_t hmdPose = m_poseOpenVR.mDeviceToAbsoluteTracking;
			affine3 hmdToCamera =
			{	// transposed from column-vector to row-vector convention
				// negate Z to transform from RH to LH coordinates
				hmdPose.m[0][0], hmdPose.m[1][0], -hmdPose.m[2][0],
				hmdPose.m[0][1], hmdPose.m[1][1], -hmdPose.m[2][1],
				hmdPose.m[0][2], hmdPose.m[1][2], -hmdPose.m[2][2],
				hmdPose.m[0][3], hmdPose.m[1][3], -hmdPose.m[2][3],
			};

			eyeToCamera = eyeToHMD * hmdToCamera;
		}

		// Calculate world-to-clip matrix for this eye
		affine3 eyeToWorld = eyeToCamera * m_camera.m_viewToWorld;
		affine3 worldToEye = transpose(eyeToWorld);

		if (ref_eyeToWorld)
			*ref_eyeToWorld = eyeToWorld;
		
		if(ref_worldToClip)
			*ref_worldToClip = affineToHomogeneous(worldToEye) * m_matProjVR[eye];

		if (ref_cameraPos)
		{
			ref_cameraPos->x = eyeToWorld.m_translation.x;
			ref_cameraPos->y = eyeToWorld.m_translation.y;
			ref_cameraPos->z = eyeToWorld.m_translation.z;
		}
	}
	else
	{
		float4x4 matProjVR;

		// tangents for DK2
		float UpTan = 1.3316f;
		float DownTan = 1.3316f;
		float LeftTan = eye == 0 ? 1.0586f : 1.0924f;
		float RightTan = eye != 0 ? 1.0586f : 1.0924f;

		matProjVR = perspProjD3DStyleReverse(-LeftTan, RightTan, -DownTan, UpTan, g_zNear);

		float shift = 0.0320000015f;
		float3 right = m_camera.m_viewToWorld.m_linear[0] * shift;

		point3 cameraPos = m_camera.m_pos;
		if (eye == 0)
		{
			cameraPos -= right;
		}
		else
		{
			cameraPos += right;
		}

		affine3 viewToWorld = m_camera.m_viewToWorld;
		viewToWorld.m_translation = makefloat3(cameraPos.x, cameraPos.y, cameraPos.z);
		affine3 worldToView = transpose(viewToWorld);

		if (ref_eyeToWorld)
			*ref_eyeToWorld = viewToWorld;

		if(ref_worldToClip)
			*ref_worldToClip = affineToHomogeneous(worldToView) * matProjVR;

		if (ref_cameraPos)
			*ref_cameraPos = cameraPos;
	}
}

void VRWorksSample::RenderScene()
{
	CalculateProjectionMatrices();

	float2 taaViewportOffset = makefloat2(0.f);
	if (g_temporalAA)
	{
		RenderTarget tmp = m_rtScene;
		m_rtScene = m_rtScenePrev;
		m_rtScenePrev = tmp;

		const float2 offsets[] = {
			makefloat2(0.0625f, -0.1875f), makefloat2(-0.0625f, 0.1875f), makefloat2(0.3125f, 0.0625f), makefloat2(-0.1875f, -0.3125f),
			makefloat2(-0.3125f, 0.3125f), makefloat2(-0.4375f, 0.0625f), makefloat2(0.1875f, 0.4375f), makefloat2(0.4375f, -0.4375f)
		};

		static int frameIndex = 0;
		frameIndex++;
		if (frameIndex >= dim(offsets))
			frameIndex = 0;

		taaViewportOffset = offsets[frameIndex];
	}

	#pragma region Shaders / state setup

	// Reset counter before drawing anything
	g_triangleCounter = 0;

	// Set up constant buffer for rendering scene
	CBFrame cbFrame;
	memset(&cbFrame, 0, sizeof(cbFrame));
	cbFrame.m_matWorldToClipPrev = m_matWorldToClipPrev;
	cbFrame.m_matWorldToClipPrevR = m_matWorldToClipPrevR;
	cbFrame.m_matWorldToUvzwShadow = m_shadowMap.m_matWorldToUvzw;
	cbFrame.m_matWorldToUvzShadowNormal = makefloat3x4(m_shadowMap.m_matWorldToUvzNormal);
	cbFrame.m_vecDirectionalLight = g_vecDirectionalLight;
	cbFrame.m_rgbDirectionalLight = g_rgbDirectionalLight;
	cbFrame.m_dimsShadowMap = makefloat2(m_shadowMap.m_dst.m_dims);
	cbFrame.m_normalOffsetShadow = g_normalOffsetShadow;
	cbFrame.m_shadowSharpening = 1.0f;
	cbFrame.m_screenSize = makefloat4(float(m_rtScene.m_dims.x), float(m_rtScene.m_dims.y), 1.0f / float(m_rtScene.m_dims.x), 1.0f / float(m_rtScene.m_dims.y));
	cbFrame.m_temporalAAClampingFactor = 1.0f;
	cbFrame.m_temporalAANewFrameWeight = g_suppressTemporalAA ? 1.f : 0.1f;
	cbFrame.m_textureLodBias = g_temporalAA ? -0.5f : 0.f;
	cbFrame.m_randomOffset = makefloat2(float(rand()), float(rand()));

	m_cbFrame.Bind(m_pCtx, CB_FRAME);

	CBVRData cbVRData;
	m_cbVRData.Bind(m_pCtx, CB_VR);
	
	float depthNear = 1.f; // 1/W projection
	float depthFar = 0.f;

	// For lens-matched shading, clear with the near plane to create a no-draw zone around the LMS boundary octagon.
	// Before rendering, the octagon will be drawn, overwriting the central area with the warped far plane depth (DrawSafeZone).
	m_pCtx->ClearDepthStencilView(m_dstSceneMSAA.m_pDsv, D3D11_CLEAR_DEPTH, g_lensMatchedShadingEnabled ? depthNear : depthFar, 0);

	m_pCtx->ClearRenderTargetView(m_rtSceneMSAA.m_pRtv, makergba(0.f));
	m_pCtx->ClearRenderTargetView(m_rtNormalsMSAA.m_pRtv, makergba(0.f));

	if (g_temporalAA)
	{
		m_pCtx->ClearRenderTargetView(m_rtMotionVectorsMSAA.m_pRtv, makergba(0.f));
		RenderTarget RTs[3] = { m_rtSceneMSAA, m_rtNormalsMSAA, m_rtMotionVectorsMSAA };
		BindMultipleRenderTargets(m_pCtx, dim(RTs), RTs, &m_dstSceneMSAA);
	}
	else
	{
		RenderTarget RTs[2] = { m_rtSceneMSAA, m_rtNormalsMSAA };
		BindMultipleRenderTargets(m_pCtx, dim(RTs), RTs, &m_dstSceneMSAA);
	}

	m_pCtx->PSSetSamplers(SAMP_DEFAULT, 1, &m_pSsTrilinearRepeatAniso);
	m_pCtx->PSSetSamplers(SAMP_SHADOW, 1, &m_pSsPCF);

	#pragma endregion

	#pragma region Regular Rendering

	if (!IsVROrFakeVRActive())
	{
		PERF_SECTION(RenderSceneMono);

		SetVRCBDataMono(&cbVRData);
		SetVRViewportsMono(taaViewportOffset);
		m_cbVRData.Update(m_pCtx, &cbVRData);

		if (g_lensMatchedShadingEnabled)
		{
			EnableLensMatchedShading(&g_lensMatchedConfig);
			DrawSafeZone();
		}

		cbFrame.m_matWorldToClip = m_camera.m_worldToClip;

		FrustumCull(cbFrame.m_matWorldToClip);

		m_cbFrame.Update(m_pCtx, &cbFrame);

		m_matWorldToClipPrev = cbFrame.m_matWorldToClip;
		m_matWorldToClipPrevR = cbFrame.m_matWorldToClipR;

		DrawRepeatedScene();
	}

	#pragma endregion

	#pragma region VR/FakeVR Rendering

	else
	{
		if (g_stereoMode == StereoMode::NONE)
		{ // render two eyes sequentially
			for (int eye = 0; eye < 2; ++eye)
			{
				PERF_SECTION(RenderSceneSingleEye);

				float4x4 worldToClip;
				point3 cameraPos;
				CalcEyeMatrices(IsVRActive(), eye, &worldToClip, &cameraPos, nullptr);

				SetVRCBDataSingleEye(eye, &cbVRData);
				SetVRViewportsSingleEye(eye, taaViewportOffset);
				
				m_cbVRData.Update(m_pCtx, &cbVRData);

				if (g_lensMatchedShadingEnabled)
				{
					EnableLensMatchedShading(&g_lensMatchedConfigVR[eye]);
					DrawSafeZone();
				}

				cbFrame.m_matWorldToClip = worldToClip;
				
				if (eye == 0)
				{
					cbFrame.m_matWorldToClipPrev = m_matWorldToClipPrev;
					m_matWorldToClipPrev = cbFrame.m_matWorldToClip;
				}
				else
				{
					cbFrame.m_matWorldToClipPrev = m_matWorldToClipPrevR;
					m_matWorldToClipPrevR = cbFrame.m_matWorldToClip;
				}

				m_cbFrame.Update(m_pCtx, &cbFrame);

				FrustumCull(cbFrame.m_matWorldToClip);

				DrawRepeatedScene();
			}
		}
		else if (g_singlePassStereoEnabled || g_instancedStereoEnabled)
		{
			PERF_SECTION(RenderSceneStereo);

			if (g_lensMatchedShadingEnabled && g_singlePassStereoEnabled)
			{
				for (int eye = 0; eye < 2; ++eye)
				{
					SetVRCBDataSingleEye(eye, &cbVRData);
					SetVRViewportsSingleEye(eye, taaViewportOffset);
					m_cbVRData.Update(m_pCtx, &cbVRData);

					EnableLensMatchedShading(&g_lensMatchedConfigVR[eye]);
					DrawSafeZone();
				}

				NvAPI_D3D_SetSinglePassStereoMode(m_pDevice, 2, 0, true);

				EnableLensMatchedShadingWithSinglePassStereo(&g_lensMatchedConfigVR[0], &g_lensMatchedConfigVR[1]);
			}
			else if (g_singlePassStereoEnabled)
			{
				NvAPI_D3D_SetSinglePassStereoMode(m_pDevice, 2, 0, true);
			}

			SetVRViewportsStereo(taaViewportOffset);

			SetVRCBDataStereo(&cbVRData);
			m_cbVRData.Update(m_pCtx, &cbVRData);

			float4x4 worldToClip;
			point3 camPos;

			CalcEyeMatrices(IsVRActive(), 0, &worldToClip, &camPos, nullptr);
			cbFrame.m_matWorldToClip = worldToClip;
			
			CalcEyeMatrices(IsVRActive(), 1, &worldToClip, &camPos, nullptr);
			cbFrame.m_matWorldToClipR = worldToClip;

			m_cbFrame.Update(m_pCtx, &cbFrame);

			m_matWorldToClipPrev = cbFrame.m_matWorldToClip;
			m_matWorldToClipPrevR = cbFrame.m_matWorldToClipR;

			FrustumCullStereo(cbFrame.m_matWorldToClip, cbFrame.m_matWorldToClipR);

			DrawRepeatedScene();
		}
	}

	#pragma endregion
	
	// Disable multiview
	if (g_singlePassStereoEnabled)
	{
		NvAPI_D3D_SetSinglePassStereoMode(m_pDevice, 1, 1, false);
	}

	// Disable lens-matched shading
	if (g_lensMatchedShadingEnabled)
	{
		DisableLensMatchedShading();
	}

	if (g_enableSSAO)
	{
		PERF_SECTION(SSAO);

		BindRenderTargets(m_pCtx, &m_rtSceneMSAA, nullptr);
		m_pCtx->PSSetShader(m_pShaderState->m_pPsAo, 0, 0);
		m_pCtx->PSSetShaderResources(0, 1, &m_dstSceneMSAA.m_pSrvDepth);
		m_pCtx->PSSetShaderResources(1, 1, &m_rtNormalsMSAA.m_pSrv);

		m_pCtx->OMSetBlendState(m_pBsMultiplicative, nullptr, ~0u);

		if (IsVROrFakeVRActive())
		{
			for (int eye = 0; eye < 2; eye++)
			{
				SetVRCBDataSingleEye(eye, &cbVRData);

				affine3 eyeToWorld;
				CalcEyeMatrices(IsVRActive(), eye, nullptr, nullptr, &eyeToWorld);
				cbFrame.m_matWorldToViewNormal = transpose(affineToHomogeneous(eyeToWorld));

				if(IsVRActive())
					cbFrame.m_matProjInv = inverse(m_matProjVR[eye]);
				else
					cbFrame.m_matProjInv = inverse(m_camera.m_projection);

				m_cbVRData.Update(m_pCtx, &cbVRData);
				m_cbFrame.Update(m_pCtx, &cbFrame);

				// use the planar viewport to cover all cases
				SetVRViewports(m_pCtx, g_planarDataVR[eye].Viewports, taaViewportOffset);

				DrawFullscreenPass(m_pCtx);
			}
		}
		else
		{
			cbFrame.m_matWorldToViewNormal = transpose(affineToHomogeneous(m_camera.m_viewToWorld));
			cbFrame.m_matProjInv = inverse(m_camera.m_projection);
			m_cbFrame.Update(m_pCtx, &cbFrame);
			DrawFullscreenPass(m_pCtx);
		}

		m_pCtx->OMSetBlendState(nullptr, nullptr, ~0u);
	}

	if (g_temporalAA)
	{
		PERF_SECTION(TemporalAA);

		if(g_multiResEnabled || g_lensMatchedShadingEnabled)
			m_pCtx->ClearRenderTargetView(m_rtScene.m_pRtv, makergba(0.f));

		m_pCtx->OMSetRenderTargets(0, nullptr, nullptr);

		ID3D11UnorderedAccessView* pUAVs[1];
		pUAVs[0] = m_rtScene.m_pUav;
		m_pCtx->CSSetUnorderedAccessViews(0, dim(pUAVs), pUAVs, nullptr);

		ID3D11ShaderResourceView* pSRVs[3];
		pSRVs[0] = m_rtSceneMSAA.m_pSrv;
		pSRVs[1] = m_rtMotionVectorsMSAA.m_pSrv;
		pSRVs[2] = m_rtScenePrev.m_pSrv;
		m_pCtx->CSSetShaderResources(0, dim(pSRVs), pSRVs);

		//m_pCtx->CSSetShader(m_pShaderState->m_pCsTemporalAA, nullptr, 0);
		m_pCtx->CSSetShader(m_pShaderState->m_pCsMSAA, nullptr, 0);
		m_pCtx->CSSetSamplers(0, 1, &m_pSsBilinearClamp);

		if (IsVROrFakeVRActive() && (g_lensMatchedShadingEnabled || g_multiResEnabled))
		{
			// Two dispatches, one per eye

			for (int eye = 0; eye < 2; eye++)
			{
				SetVRCBDataSingleEye(eye, &cbVRData);

				m_cbVRData.Update(m_pCtx, &cbVRData);

				int2 viewportSize = makeint2(m_rtSceneMSAA.m_dims.x / 2, m_rtSceneMSAA.m_dims.y);

				cbFrame.m_viewportOrigin = makefloat2(float(viewportSize.x) * eye, 0);
				m_cbFrame.Update(m_pCtx, &cbFrame);

				m_pCtx->Dispatch((viewportSize.x + 15) / 16, (viewportSize.y + 15) / 16, 1);
			}
		}
		else
		{
			cbFrame.m_viewportOrigin = makefloat2(0, 0);
			m_cbFrame.Update(m_pCtx, &cbFrame);

			m_pCtx->Dispatch((m_rtScene.m_dims.x + 15) / 16, (m_rtScene.m_dims.y + 15) / 16, 1);
		}

		memset(pUAVs, 0, sizeof(pUAVs));
		m_pCtx->CSSetUnorderedAccessViews(0, dim(pUAVs), pUAVs, nullptr);
	}
	else
	{
		// Resolve the MSAA buffer
		m_pCtx->ResolveSubresource(m_rtScene.m_pTex, 0, m_rtSceneMSAA.m_pTex, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	}
		
	g_suppressTemporalAA = false;
}

void VRWorksSample::DrawRepeatedScene()
{
	m_pCtx->VSSetShader(m_pShaderState->m_pVsWorld, 0, 0);
	m_pCtx->GSSetShader(m_pShaderState->m_pGsWorld, 0, 0);

	m_pCtx->OMSetDepthStencilState(m_pDssDepthTestReverse, 0);

	for (int i = 0; i < g_repeatRenderingCount; ++i)
	{
		DrawObjects(m_pShaderState->m_pPsForward, m_pShaderState->m_pPsForward, false);
	}

	m_pCtx->VSSetShader(nullptr, 0, 0);
	m_pCtx->GSSetShader(nullptr, 0, 0);
}

void VRWorksSample::RenderShadowMap()
{
	// Calculate shadow map matrices
	float4x4 matWorldToClipPrev = m_shadowMap.m_matWorldToClip;
	m_shadowMap.m_vecLight = g_vecDirectionalLight;

	// hardcode some limits in order not to calculate the min/max from all objects' instances
	m_shadowMap.m_boundsScene.m_mins.x = -20.f;
	m_shadowMap.m_boundsScene.m_mins.y = -10.f;
	m_shadowMap.m_boundsScene.m_mins.z = 0.f;
	m_shadowMap.m_boundsScene.m_maxs.x = 10.f;
	m_shadowMap.m_boundsScene.m_maxs.y = 20.f;
	m_shadowMap.m_boundsScene.m_maxs.z = 30.f;

	m_shadowMap.UpdateMatrix();

	// Only re-render if the matrix changed, to save perf
	if (all(isnear(m_shadowMap.m_matWorldToClip, matWorldToClipPrev)))
		return;

	PERF_SECTION(RenderShadowMap);

	m_pCtx->OMSetDepthStencilState(m_pDssDepthTest, 0);

	// Set up constant buffer for rendering to shadow map
	CBFrame cbFrame =
	{
		m_shadowMap.m_matWorldToClip,
	};
	m_cbFrame.Update(m_pCtx, &cbFrame);
	m_cbFrame.Bind(m_pCtx, CB_FRAME);

	m_pCtx->ClearDepthStencilView(m_shadowMap.m_dst.m_pDsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_shadowMap.Bind(m_pCtx);

	m_pCtx->VSSetShader(m_pShaderState->m_pVsShadow, nullptr, 0);
	m_pCtx->PSSetSamplers(SAMP_DEFAULT, 1, &m_pSsTrilinearRepeatAniso);

	FrustumCull(cbFrame.m_matWorldToClip);
	DrawObjects(nullptr, m_pShaderState->m_pPsShadowAlphaTest, true);
}



void VRWorksSample::RecalcVRData()
{
	g_projectionDataPrev = g_projectionData;
	g_projectionDataVRPrev[0] = g_projectionDataVR[0];
	g_projectionDataVRPrev[1] = g_projectionDataVR[1];

	Nv::VR::Viewport boundingBox(0, 0, m_rtScene.m_dims.x, m_rtScene.m_dims.y);
	Nv::VR::Viewport boundingBoxVR[2] =
	{
		Nv::VR::Viewport(0.f,                   0.f, boundingBox.Width / 2, boundingBox.Height),
		Nv::VR::Viewport(boundingBox.Width / 2, 0.f, boundingBox.Width / 2, boundingBox.Height)
	};

	Nv::VR::Float2 flattenedSize = Nv::VR::Float2(float(m_rtUpscaled.m_dims.x), float(m_rtUpscaled.m_dims.y));
	flattenedSize.x *= g_resolutionScale;
	flattenedSize.y *= g_resolutionScale;

	Nv::VR::Float2 flattenedSizeVR = flattenedSize;
	flattenedSizeVR.x *= 0.5f;

	// Planar
	Nv::VR::Planar::Configuration dummyConfig;
	CalculateViewportsAndBufferData(flattenedSize, boundingBox, dummyConfig, g_planarData);
	CalculateViewportsAndBufferData(flattenedSizeVR, boundingBoxVR[0], dummyConfig, g_planarDataVR[0]);
	CalculateViewportsAndBufferData(flattenedSizeVR, boundingBoxVR[1], dummyConfig, g_planarDataVR[1]);

	if (g_multiResEnabled)
	{
		// Multi-Res Shading
		CalculateViewportsAndBufferData(flattenedSize, boundingBox, g_multiResConfig, g_projectionData);
		CalculateMirroredConfig(g_multiResConfigVR[0] = g_multiResConfig, g_multiResConfigVR[1]);
		CalculateViewportsAndBufferData(flattenedSizeVR, boundingBoxVR[0], g_multiResConfigVR[0], g_projectionDataVR[0]);
		CalculateViewportsAndBufferData(flattenedSizeVR, boundingBoxVR[1], g_multiResConfigVR[1], g_projectionDataVR[1]);
	}
	else if (g_lensMatchedShadingEnabled)
	{
		// Lens-Matched Shading
		CalculateViewportsAndBufferData(flattenedSize, boundingBox, g_lensMatchedConfig, g_projectionData);
		CalculateMirroredConfig(g_lensMatchedConfigVR[0] = g_lensMatchedConfig, g_lensMatchedConfigVR[1]);
		CalculateViewportsAndBufferData(flattenedSizeVR, boundingBoxVR[0], g_lensMatchedConfigVR[0], g_projectionDataVR[0]);
		CalculateViewportsAndBufferData(flattenedSizeVR, boundingBoxVR[1], g_lensMatchedConfigVR[1], g_projectionDataVR[1]);
	}
	else
	{
		g_projectionData = g_planarData;
		g_projectionDataVR[0] = g_planarDataVR[0];
		g_projectionDataVR[1] = g_planarDataVR[1];
	}

#if 0
	// Test the MapClipToWindow / MapWindowToClip functions
	POINT cursorPos;
	if (GetCursorPos(&cursorPos) && ScreenToClient(m_hWnd, &cursorPos))
	{
		Float2 windowPos{ float(cursorPos.x), float(cursorPos.y) };
		Float2 clipPos = MapWindowToClip(g_lensMatchedData, windowPos);
		Float2 reconstructedWindowPos = MapClipToWindow(g_lensMatchedData, clipPos);
		ASSERT_ERR(abs(windowPos.x - reconstructedWindowPos.x) < 0.001f && abs(windowPos.y - reconstructedWindowPos.y) < 0.001f);
	}
#endif
}

void VRWorksSample::SetVRCBData(const Nv::VR::Data& data, const Nv::VR::Data& dataR, const Nv::VR::Data& dataPrev, const Nv::VR::Data& dataPrevR, const int2_arg dimsRt, CBVRData* ref_cbData)
{
	ASSERT_ERR(all(dimsRt > 0));
	ASSERT_ERR(ref_cbData);

	ref_cbData->m_vrFastGSCBData = data.FastGsCbData;
	ref_cbData->m_vrRemapCBData = data.RemapCbData;

	ref_cbData->m_vrFastGSCBDataR = dataR.FastGsCbData;
	ref_cbData->m_vrRemapCBDataR = dataR.RemapCbData;

	ref_cbData->m_vrRemapCBDataPrev = dataPrev.RemapCbData;
	ref_cbData->m_vrRemapCBDataPrevR = dataPrevR.RemapCbData;
}

void VRWorksSample::SetVRViewports(ID3D11DeviceContext* context, const Nv::VR::ProjectionViewports& viewports, float2 viewportOffset)
{
	D3D11_VIEWPORT viewportsDx[Nv::VR::ProjectionViewports::MaxCount];
	D3D11_RECT rectsDx[Nv::VR::ProjectionViewports::MaxCount];

	for (int i = 0; i < viewports.NumViewports; ++i)
	{
		viewportsDx[i] = D3D11_VIEWPORT{
			viewports.Viewports[i].TopLeftX + viewportOffset.x,
			viewports.Viewports[i].TopLeftY + viewportOffset.y,
			viewports.Viewports[i].Width,
			viewports.Viewports[i].Height,
			viewports.Viewports[i].MinDepth,
			viewports.Viewports[i].MaxDepth
		};
		rectsDx[i] = D3D11_RECT{
			viewports.Scissors[i].Left,
			viewports.Scissors[i].Top,
			viewports.Scissors[i].Right,
			viewports.Scissors[i].Bottom,
		};
	}

	context->RSSetViewports(viewports.NumViewports, viewportsDx);
	context->RSSetScissorRects(viewports.NumViewports, rectsDx);
}

void VRWorksSample::DrawSplits(const Nv::VR::Data& data, RenderTarget* pRt, int Width, int Height)
{
	static const rgba s_rgbaSplits = { 0.0f, 1.0f, 1.0f, 1.0f };

	auto ScreenToClip = [pRt](int x, int y)
	{
		return makepoint2(float(x) / float(pRt->m_dims.x) * 2.f - 1.f, -float(y) / float(pRt->m_dims.y) * 2.f + 1.f);
	};

	for (int x = 1; x < Width; x++)
	{
		for (int y = 1; y < Height; y++)
		{
			const Nv::VR::ScissorRect& scissor = data.Viewports.Scissors[y * Width + x];
			AddDebugLine(ScreenToClip(data.Viewports.BoundingRect.Left, scissor.Top), ScreenToClip(data.Viewports.BoundingRect.Right, scissor.Top), s_rgbaSplits);
			AddDebugLine(ScreenToClip(scissor.Left, data.Viewports.BoundingRect.Top), ScreenToClip(scissor.Left, data.Viewports.BoundingRect.Bottom), s_rgbaSplits);
		}
	}

	pRt->Bind(m_pCtx);
	DrawDebugLines(m_pCtx);
}

void VRWorksSample::DrawSplits()
{
	int WidthOrHeight = g_multiResEnabled ? 3 : 2;

	if (g_drawMultiResSplits && g_multiResEnabled || g_drawLMSSplits && g_lensMatchedShadingEnabled)
	{
		PERF_SECTION(DrawSplits);

		if (IsVROrFakeVRActive())
		{
			DrawSplits(g_projectionDataVR[0], &m_rtScene, WidthOrHeight, WidthOrHeight);
			DrawSplits(g_projectionDataVR[1], &m_rtScene, WidthOrHeight, WidthOrHeight);
		}
		else
		{
			DrawSplits(g_projectionData, &m_rtScene, WidthOrHeight, WidthOrHeight);
		}
	}
}

void VRWorksSample::SetVRCBDataMono(CBVRData* ref_cbData)
{
	SetVRCBData(g_projectionData, g_projectionData, g_projectionDataPrev, g_projectionDataPrev, m_rtScene.m_dims, ref_cbData);
}

void VRWorksSample::SetVRCBDataSingleEye(int eye, CBVRData* ref_cbData)
{
	SetVRCBData(g_projectionDataVR[eye], g_projectionDataVR[eye], g_projectionDataVRPrev[eye], g_projectionDataVRPrev[eye], m_rtScene.m_dims, ref_cbData);
}

void VRWorksSample::SetVRCBDataStereo(CBVRData* ref_cbData)
{
	SetVRCBData(g_projectionDataVR[0], g_projectionDataVR[1], g_projectionDataVRPrev[0], g_projectionDataVRPrev[1], m_rtScene.m_dims, ref_cbData);
}

void VRWorksSample::SetVRViewportsMono(float2 viewportOffset)
{
	SetVRViewports(m_pCtx, g_projectionData.Viewports, viewportOffset);
}

void VRWorksSample::SetVRViewportsSingleEye(int eye, float2 viewportOffset)
{
	SetVRViewports(m_pCtx, g_projectionDataVR[eye].Viewports, viewportOffset);
}

void VRWorksSample::SetVRViewportsStereo(float2 viewportOffset)
{
	if (g_instancedStereoEnabled)
	{
		SetViewport(m_pCtx, viewportOffset, makefloat2(float(m_rtSceneMSAA.m_dims.x), float(m_rtSceneMSAA.m_dims.y)));
	}
	else
	{
		Nv::VR::ProjectionViewports stereoViewports;
		Nv::VR::ProjectionViewports::Merge(g_projectionDataVR[0].Viewports, g_projectionDataVR[1].Viewports, stereoViewports);
		SetVRViewports(m_pCtx, stereoViewports, viewportOffset);
	}
}

void VRWorksSample::EnableLensMatchedShading(const Nv::VR::LensMatched::Configuration* config)
{
	if (g_featureLevel < NvFeatureLevel::PASCAL_GPU)
		return;

	float left  = -config->WarpLeft;
	float right =  config->WarpRight;
	float up    =  config->WarpUp;
	float down  = -config->WarpDown;
	
	NV_MODIFIED_W_PARAMS modifiedWParams = { 0 };
	modifiedWParams.version = NV_MODIFIED_W_PARAMS_VER;
	modifiedWParams.numEntries = 4;

	// Scale factors (W` = W + Ax + By)

	modifiedWParams.modifiedWCoefficients[0].fA = left;
	modifiedWParams.modifiedWCoefficients[0].fB = up;

	modifiedWParams.modifiedWCoefficients[1].fA = right;
	modifiedWParams.modifiedWCoefficients[1].fB = up;

	modifiedWParams.modifiedWCoefficients[2].fA = left;
	modifiedWParams.modifiedWCoefficients[2].fB = down;

	modifiedWParams.modifiedWCoefficients[3].fA = right;
	modifiedWParams.modifiedWCoefficients[3].fB = down;

	NvAPI_Status Status = NVAPI_OK;
	Status = NvAPI_D3D_SetModifiedWMode(m_pDevice, &modifiedWParams);
	ASSERT_ERR(Status == NVAPI_OK);
}

void VRWorksSample::EnableLensMatchedShadingWithSinglePassStereo(const Nv::VR::LensMatched::Configuration* leftConfig, const Nv::VR::LensMatched::Configuration* rightConfig)
{
	if (g_featureLevel < NvFeatureLevel::PASCAL_GPU)
		return;

	NV_MODIFIED_W_PARAMS modifiedWParams = { 0 };
	modifiedWParams.version = NV_MODIFIED_W_PARAMS_VER;
	modifiedWParams.numEntries = 8;

	for (int i = 0; i < 2; ++i)
	{
		const Nv::VR::LensMatched::Configuration* config = i == 0 ? leftConfig : rightConfig;

		float left  = -config->WarpLeft;
		float right =  config->WarpRight;
		float up    =  config->WarpUp;
		float down  = -config->WarpDown;
	
		int ind = i * 4;

		modifiedWParams.modifiedWCoefficients[ind + 0].fA = left;
		modifiedWParams.modifiedWCoefficients[ind + 0].fB = up;

		modifiedWParams.modifiedWCoefficients[ind + 1].fA = right;
		modifiedWParams.modifiedWCoefficients[ind + 1].fB = up;

		modifiedWParams.modifiedWCoefficients[ind + 2].fA = left;
		modifiedWParams.modifiedWCoefficients[ind + 2].fB = down;

		modifiedWParams.modifiedWCoefficients[ind + 3].fA = right;
		modifiedWParams.modifiedWCoefficients[ind + 3].fB = down;
	}

	NvAPI_Status Status = NVAPI_OK;
	Status = NvAPI_D3D_SetModifiedWMode(m_pDevice, &modifiedWParams);
	ASSERT_ERR(Status == NVAPI_OK);
}

void VRWorksSample::DisableLensMatchedShading()
{
	if (g_featureLevel < NvFeatureLevel::PASCAL_GPU)
		return;

	NV_MODIFIED_W_PARAMS modifiedWParams = { 0 };
	modifiedWParams.version = NV_MODIFIED_W_PARAMS_VER;
	modifiedWParams.numEntries = 0;

	NvAPI_Status Status = NVAPI_OK;
	Status = NvAPI_D3D_SetModifiedWMode(m_pDevice, &modifiedWParams);
	ASSERT_ERR(Status == NVAPI_OK);
}

void VRWorksSample::DrawSafeZone()
{
	PERF_SECTION(DrawSafeZone);

	m_pCtx->RSSetState(m_pRsDefault);
	m_pCtx->VSSetShader(m_pShaderState->m_pVsSafeZone, nullptr, 0);
	m_pCtx->GSSetShader(m_pShaderState->m_pGsSafeZone, nullptr, 0);
	m_pCtx->PSSetShader(nullptr, nullptr, 0);
	m_pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pCtx->IASetInputLayout(nullptr);
	m_pCtx->OMSetDepthStencilState(m_pDssDepthWriteNoDepthTest, 0);

	m_pCtx->Draw(6, 0);

	m_pCtx->VSSetShader(nullptr, nullptr, 0);
	m_pCtx->GSSetShader(nullptr, nullptr, 0);
}

void VRWorksSample::FlattenImage()
{
	PERF_SECTION(FlattenImage);

	static const FLOAT color[] = { 1, 1, 0, 0 };
	m_pCtx->ClearRenderTargetView(m_rtUpscaled.m_pRtv, color);

	m_rtUpscaled.Bind(m_pCtx);
	m_pCtx->PSSetShader(m_pShaderState->m_pPsFlatten, nullptr, 0);

	m_pCtx->PSSetShaderResources(0, 1, &m_rtScene.m_pSrv);

	m_pCtx->PSSetSamplers(0, 1, &m_pSsBilinearClamp);

	if (!IsVROrFakeVRActive())
	{
		CBVRData cbVR;
		SetVRCBDataMono(&cbVR);
		m_cbVRData.Update(m_pCtx, &cbVR);
		m_cbVRData.Bind(m_pCtx, CB_VR);

		DrawFullscreenPass(m_pCtx);
	}
	else
	{
		for (int eye = 0; eye < 2; ++eye)
		{
			CBVRData cbVR;
			SetVRCBDataSingleEye(eye, &cbVR);
			m_cbVRData.Update(m_pCtx, &cbVR);
			m_cbVRData.Bind(m_pCtx, CB_VR);

			DrawRectPass(m_pCtx, makebox2(0.5f * eye, 0.0f, 0.5f + 0.5f * eye, 1.0f));
		}
	}	

	m_pCtx->PSSetShader(nullptr, nullptr, 0);
}

void VRWorksSample::CalculateProjectionMatrices()
{
	if (m_oculusSession)
	{
		// Set new projection matrices
		for (int i = 0; i < 2; ++i)
		{
			m_matProjVR[i] = perspProjD3DStyleReverse(
				-m_eyeFovOculusHMD[i].LeftTan,
				m_eyeFovOculusHMD[i].RightTan,
				-m_eyeFovOculusHMD[i].DownTan,
				m_eyeFovOculusHMD[i].UpTan,
				g_zNear);
		}
	}
	else if (m_pOpenVRSystem)
	{
		// Set new projection matrices
		for (int i = 0; i < 2; ++i)
		{
			// Use GetProjectionRaw instead of GetProjectionMatrix because we use 1/W projection here
			float left, right, top, bottom;
			m_pOpenVRSystem->GetProjectionRaw(vr::EVREye(i), &left, &right, &top, &bottom);
			m_matProjVR[i] = perspProjD3DStyleReverse(left, right, -bottom, -top, g_zNear);
		}
	}
	else
	{
		// Update projection matrix for new aspect ratio
		float fov = 1.0f;
		float aspect = float(m_rtUpscaled.m_dims.x) / float(m_rtUpscaled.m_dims.y);

		m_camera.SetReverseProjection(fov, aspect, g_zNear);
	}
}

bool VRWorksSample::TryActivateVR()
{
	if (IsVRActive())
		return true;

	// Detect Oculus headset
	ovrHmdDesc oculusHmdDesc = ovr_GetHmdDesc(nullptr);
	if (oculusHmdDesc.Type != ovrHmd_None)
	{
		if (TryActivateOculusVR())
		{
			return true;
		}
		else
		{
			DeactivateOculusVR();
			return false;
		}
	}

	// Detect OpenVR headset
	if (vr::VR_IsHmdPresent())
	{
		if (TryActivateOpenVR())
		{
			return true;
		}
		else
		{
			DeactivateOpenVR();
			return false;
		}
	}

	ERR("No VR headset detected");
	return false;
}

void VRWorksSample::DeactivateVR()
{
	if (m_oculusSession)
		DeactivateOculusVR();
	else if (m_pOpenVRSystem)
		DeactivateOpenVR();

	if (!IsVROrFakeVRActive())
	{
		g_instancedStereoEnabled = false;
		g_singlePassStereoEnabled = false;
	}
}

bool VRWorksSample::TryActivateOculusVR()
{
	// Connect to HMD
	ovrGraphicsLuid oculusLuid = {};
	if (OVR_FAILURE(ovr_Create(&m_oculusSession, &oculusLuid)))
		return false;
	ASSERT_ERR(m_oculusSession);
	if (!m_oculusSession)
		return false;

	// Check that the adapter LUID matches our DX11 device
	comptr<IDXGIDevice> pDXGIDevice;
	CHECK_D3D(m_pDevice->QueryInterface<IDXGIDevice>(&pDXGIDevice));
	comptr<IDXGIAdapter> pAdapter;
	CHECK_D3D(pDXGIDevice->GetAdapter(&pAdapter));
	DXGI_ADAPTER_DESC adapterDesc;
	CHECK_D3D(pAdapter->GetDesc(&adapterDesc));
	adapterDesc.AdapterLuid;
	cassert(sizeof(adapterDesc.AdapterLuid) == sizeof(oculusLuid));
	if (memcmp(&adapterDesc.AdapterLuid, &oculusLuid, sizeof(oculusLuid)) != 0)
	{
		ERR("Oculus VR headset is connected to a different GPU than the app is running on.");
		return false;
	}

	// Create swap texture set for both eyes side-by-side
	ovrHmdDesc hmdDesc = ovr_GetHmdDesc(m_oculusSession);
	ovrSizei eyeSizeLeft = ovr_GetFovTextureSize(m_oculusSession, ovrEye_Left, hmdDesc.DefaultEyeFov[0], 1.0f);
	ovrSizei eyeSizeRight = ovr_GetFovTextureSize(m_oculusSession, ovrEye_Right, hmdDesc.DefaultEyeFov[1], 1.0f);
	g_vrSwapChainSize = { max(eyeSizeLeft.w, eyeSizeRight.w) * 2, max(eyeSizeLeft.h, eyeSizeRight.h) };
	 
	ovrTextureSwapChainDesc swapDesc =
	{
		ovrTexture_2D,
		OVR_FORMAT_R8G8B8A8_UNORM_SRGB,
		1, g_vrSwapChainSize.x, g_vrSwapChainSize.y, 1, 1,
		ovrFalse,
		ovrTextureMisc_DX_Typeless,
		ovrTextureBind_DX_RenderTarget
	};
	CHECK_OVR(ovr_CreateTextureSwapChainDX(
		m_oculusSession, m_pDevice, &swapDesc, &m_oculusTextureSwapChain
	));
	ASSERT_ERR(m_oculusTextureSwapChain);


	if (!m_oculusTextureSwapChain)
		return false;

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = { };
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.Texture2D.MipSlice = 0;

	m_oculusSwapTextureRTVs.clear();

	int swapLength = 0;
	ovr_GetTextureSwapChainLength(m_oculusSession, m_oculusTextureSwapChain, &swapLength);
	m_oculusSwapTextureRTVs.resize(swapLength);
	for (int nTexture = 0; nTexture < swapLength; nTexture++)
	{
		ID3D11Texture2D* swapTex = nullptr;
		ovr_GetTextureSwapChainBufferDX(m_oculusSession, m_oculusTextureSwapChain, nTexture, IID_PPV_ARGS(&swapTex));
		CHECK_D3D(m_pDevice->CreateRenderTargetView(swapTex, &rtvDesc, &m_oculusSwapTextureRTVs[nTexture]));
	}

	for (int i = 0; i < 2; ++i)
	{
		// Store the eye FOVs and offset vectors for later use
		m_eyeFovOculusHMD[i] = hmdDesc.DefaultEyeFov[i];
		ovrEyeRenderDesc eyeRenderDesc = ovr_GetRenderDesc(m_oculusSession, ovrEyeType(i), hmdDesc.DefaultEyeFov[i]);
		m_eyeOffsetsOculusHMD[i] = eyeRenderDesc.HmdToEyeOffset;
	}

	return true;
}

void VRWorksSample::DeactivateOculusVR()
{
	m_oculusSwapTextureRTVs.clear();

	if (m_oculusTextureSwapChain)
	{
		ovr_DestroyTextureSwapChain(m_oculusSession, m_oculusTextureSwapChain);
		m_oculusTextureSwapChain = nullptr;
	}

	if (m_oculusSession)
	{
		ovr_Destroy(m_oculusSession);
		m_oculusSession = nullptr;
	}
}

bool VRWorksSample::TryActivateOpenVR()
{
	// Loading the SteamVR Runtime
	vr::EVRInitError initError;
	m_pOpenVRSystem = vr::VR_Init(&initError, vr::VRApplication_Scene);
	if (initError != vr::VRInitError_None)
	{
		ERR("VR_Init failed with error code: %d\nError message: %s", initError, vr::VR_GetVRInitErrorAsEnglishDescription(initError));
		return false;
	}

	// Init compositor.
	m_pOpenVRCompositor = (vr::IVRCompositor *)vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &initError);
	if (initError != vr::VRInitError_None)
	{
		ERR("VR_GetGenericInterface failed with error code: %d\nError message: %s", initError, vr::VR_GetVRInitErrorAsEnglishDescription(initError));
		return false;
	}

	// Set new render target size for side-by-side stereo
	uint32_t eyeWidth, eyeHeight;
	m_pOpenVRSystem->GetRecommendedRenderTargetSize(&eyeWidth, &eyeHeight);
	g_vrSwapChainSize = makeint2(eyeWidth * 2, eyeHeight);

	return true;
}

void VRWorksSample::DeactivateOpenVR()
{
	if (m_pOpenVRSystem)
	{
		vr::VR_Shutdown();
		m_pOpenVRSystem = nullptr;
		m_pOpenVRCompositor = nullptr;
	}
}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Get the whole shebang going

	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	VRWorksSample w;
	if (!w.Init(hInstance))
	{
		w.Shutdown();
		return 1;
	}

	w.MainLoop(SW_SHOWMAXIMIZED);
	return 0;
}
