NVIDIA VR SLI v2.2 --- 2016-10-26
---------------------------------

This package contains the following:
- nvapi/:
        NVAPI headers and libs; VR SLI API definitions are in nvapi.h.
- doc/NVIDIA_VR_SLI_Programmer's_Guide.pdf:
        VR SLI documentation.
- demo/:
        Demo app that renders Crytek Sponza test scene in stereo using VR SLI;
        also supports Oculus and OpenVR headsets.

System requirements:
- Windows 7 or later
- SLI system with GeForce GTX 650 or higher GPUs
- SLI bridge is optional
- Visual Studio 2013 (any edition) is needed to build the demo app

Installation:
- Install NVIDIA graphics driver 375.00 or later.
- After installing the driver, ensure SLI mode is enabled in the NV control panel:
    3D Settings -> Configure SLI -> select "Maximize 3D performance"

Changes since previous release
------------------------------
- Updated sample app to NVAPI R375
- Updated to LibOVR 1.9.0
- Updated to OpenVR 1.0.3

Known issues
------------
The sample app depends on the working directory being the same directory as the crytek_sponza folder, so run directly from the demo/ folder or using Visual Studio

Contact Info
------------
Please send any questions, comments or bug reports to vrsupport@nvidia.com. If submitting a bug
report, please note your OS, GPU model, and driver version.



Previous Releases
-----------------

NVIDIA VR SLI v2.1 --- 2016-09-07
---------------------------------

Changes since previous release
------------------------------
- Updated sample app to NVAPI R370

NVIDIA VR SLI v2.0 --- 2016-05-13
---------------------------------

Changes since previous release
------------------------------
- Removed HMD polling code as it is not meant to be called every frame and was introducing stutter
- Updated sample app to NVAPI R367
- Updated sample app to OpenVR 0.9.20

NVIDIA VR SLI v1.3 --- 2016-03-28
---------------------------------

Changes since previous release
------------------------------
- Updated sample app to NVAPI R364
- Updated sample app to LibOVR 1.3
- Updated sample app to OpenVR 0.9.19

NVIDIA VR SLI v1.2 --- 2016-02-02
---------------------------------

Changes since previous release
------------------------------
- Updated sample app to OpenVR 0.9.14.

NVIDIA VR SLI v1.1 --- 2015-12-21
---------------------------------

Changes since previous release
------------------------------
- Added NvAPI_D3D_ImplicitSLIControl(), which can be used to programmatically disable automatic AFR
  SLI, overriding app profile or control panel SLI settings. This can (and should) be used by any VR
  application, even if not using VR SLI, to ensure AFR is not accidentally enabled in VR.
- Added D3D11 deferred context support to the VR SLI API. See SetContextGPUMask() in docs.
- Updated sample app to OpenVR 0.9.12.

Fixed issues since previous release
-----------------------------------
- Fixed sample app causing D3D11 debug layer errors when repeated rendering is enabled.

NVIDIA VR SLI v1.0 --- 2015-11-19
---------------------------------

Changes since previous release
------------------------------
- Updated sample app to Oculus SDK 0.8.0.0 and OpenVR 0.9.11.
- Sample app now includes "Repeat Rendering" control to increase GPU load for performance testing.
    Also, sample app no longer includes San Miguel scene, only Crytek Sponza scene.

Fixed issues since previous release
-----------------------------------
- Fixed incorrect rendering when DX11.2 CopyTiles and UpdateTiles APIs were used with VR SLI.

Known issues
------------
- While a VR SLI app is running, extra command packets are sometimes seen on secondary GPUs, from
    other applications that do not use VR SLI.
- D3D11 deferred contexts are not fully supported by the VR SLI API.

NVIDIA VR SLI beta 4 --- 2015-10-19
-----------------------------------

Fixed issues since previous release
-----------------------------------
- Updated sample app to Oculus SDK 0.7.0.0 and OpenVR 0.9.10.
- Fixed ID3D11MultiGPUDevice::CopySubresourceRegion sometimes returning a bogus
    NVAPI_INVALID_ARGUMENT error (-5), due to a bug in bounds-checking the copy rectangle.
- VR SLI is now compatible with NVIDIA driver direct mode, pre-Win10. (Still working on Win10.)
- Fixed driver crash when setting compute shader constant buffers.
- Fixed driver crash in ID3D11MultiGPUDevice::CopySubresourceRegion.
- Addressed performance issues on Quadro GPUs.
- Addressed premature out-of-memory errors when allocating large numbers of VRAM resources.
- Fixed very occasional heap corruption seen with long-running VR SLI apps.

Known issues
------------
- D3D11 deferred contexts are not fully supported by the VR SLI API.

NVIDIA VR SLI beta 2 --- 2015-08-13
-----------------------------------

Known issues
------------
- ID3D11MultiGPUDevice::UpdateTiles() is not yet implemented.
