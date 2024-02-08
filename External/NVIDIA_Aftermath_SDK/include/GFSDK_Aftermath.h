/*
* Copyright (c) 2017-2023, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

/*
*   █████  █████ ██████ ████  ████   ███████   ████  ██████ ██   ██
*   ██  ██ ██      ██   ██    ██  ██ ██ ██ ██ ██  ██   ██   ██   ██
*   ██  ██ ██      ██   ██    ██  ██ ██ ██ ██ ██  ██   ██   ██   ██
*   ██████ ████    ██   ████  █████  ██ ██ ██ ██████   ██   ███████
*   ██  ██ ██      ██   ██    ██  ██ ██    ██ ██  ██   ██   ██   ██
*   ██  ██ ██      ██   █████ ██  ██ ██    ██ ██  ██   ██   ██   ██   DEBUGGER
*                                                           ██   ██
*  ████████████████████████████████████████████████████████ ██ █ ██ ████████████
*
*
*  HOW TO USE AFTERMATH for DX11 and DX12
*  --------------------------------------
*
*  NOTE: Some of the Aftermath 1.x functionality will go away in a future release.
*        The functions and structures to be removed are indicated with a DEPRECATED
*        comment. The supported method for accessing this data is provided via the
*        GPU crash dump functionality. Please refer to the
*        'GFSDK_Aftermath_GpuCrashDump.h' header file for more details.
*
*  NOTE: Aftermath does not support UWP applications.
*
*  Call 'GFSDK_Aftermath_DXxx_Initialize', to initialize the library and to enable
*  the desired Aftermath feature set. See 'GFSDK_Aftermath_FeatureFlags' below for
*  the list of supported features.
*  This must be done before any other library calls are made, and the method must
*  return 'GFSDK_Aftermath_Result_Success' for initialization to be complete.
*
*  Initialization of Aftermath may fail for a variety of reasons, including:
*
*  o) The initialization function was already called for the device:
*       'GFSDK_Aftermath_Result_FAIL_AlreadyInitialized'.
*
*
*  o) Aftermath isn't supported on the GPU associated with the device or the NVIDIA
*     display driver version installed:
*       'GFSDK_Aftermath_Result_FAIL_InvalidAdapter'
*       'GFSDK_Aftermath_Result_FAIL_DriverInitFailed',
*       'GFSDK_Aftermath_Result_FAIL_DriverVersionNotSupported',
*       'GFSDK_Aftermath_Result_FAIL_NvApiIncompatible'.
*
*
*  o) A D3D API debug layer, such as PIX or other graphics debuggers, was detected
*     that is incompatible with Aftermath:
*       'GFSDK_Aftermath_Result_FAIL_D3dDllInterceptionNotSupported'
*
*
*  o) Aftermath was disabled on the system by the current user setting the
*     'HKEY_CURRENT_USER\Software\NVIDIA Corporation\Nsight Aftermath\ForceOff'
*     Windows registry key: 'GFSDK_Aftermath_Result_FAIL_Disabled'
*
*
*  After detecting D3D device lost (TDR):
*
*  o)  To query the fault reason after TDR, use the 'GFSDK_Aftermath_GetDeviceStatus'
*      call. See 'GFSDK_Aftermath_Device_Status', for the full list of possible
*      status.
*
*
*  o)  In the event of a GPU page fault, use the 'GFSDK_Aftermath_GetPageFaultInformation'
*      method to return more information about what might of gone wrong. A GPU
*      virtual address (VA) is returned, along with the resource descriptor of the
*      resource that VA lands in.
*      NOTE: It's not 100% certain that this is the resource is related to the fault,
*      only that the faulting VA lands within this resource in memory. It is always
*      possible that due to a bug or due to faulty dependency a random GPU VA is
*      accessed by a shader.
*
*
*  Optionally, instrument the application with Aftermath event markers:
*
*  1)  For each DX12 command list or DX11 device context you expect to use with
*      Aftermath, initialize them using the 'GFSDK_Aftermath_DXxx_CreateContextHandle'
*      function. DX12 command lists must be in the recording state when this function
*      is called, but the returned context handle will remain valid through subsequent
*      command list closes and resets.
*
*
*  2)  Call 'GFSDK_Aftermath_SetEventMarker' to inject an event marker directly into
*      the command stream at that point. DX12 command lists must be in the recording
*      state when this function is called.
*
*      PERFORMANCE TIP:
*
*      Do not use 'GFSDK_Aftermath_SetEventMarker' in high frequency code paths.
*      Injecting event markers introduces considerable CPU overhead. For reduced
*      CPU overhead, use 'GFSDK_Aftermath_SetEventMarker' with 'markerDataSize = 0'.
*      This instructs Aftermath not to allocate and copy off memory internally,
*      relying on the application to manage marker pointers itself.
*
*
*  3)  Once TDR/hang occurs, call the 'GFSDK_Aftermath_GetData' API to fetch the
*      event marker last processed by the GPU for each context. This API also
*      supports fetching the current execution state for each the GPU.
*
*
*  4)  Before the app shuts down, each Aftermath context handle must be cleaned
*      up, this is done with the 'GFSDK_Aftermath_ReleaseContextHandle' call.
*
*
*
*  HOW TO USE AFTERMATH for Vulkan
*  -------------------------------
*
*  For Vulkan use the 'VK_NV_device_diagnostics_config' extension to initialize and
*  configure the Aftermath feature set to use. The meaning of the flag bits
*  defined by 'VkDeviceDiagnosticsConfigFlagBitsNV' correspond to the features
*  defined by 'GFSDK_Aftermath_FeatureFlags' for DX11/DX12 below.
*
*  Use the 'VK_NV_device_diagnostic_checkpoints' extension to add event markers into
*  the command stream.
*
*/

#ifndef GFSDK_Aftermath_H
#define GFSDK_Aftermath_H

#include "GFSDK_Aftermath_Defines.h"

#pragma pack(push, 8)

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_FeatureFlags
// ---------------------------------
//
// Feature flags that can be used to enabled various features when initializing DX
// Aftermath via the 'GFSDK_Aftermath_DX11_Initialize' and
// 'GFSDK_Aftermath_DX12_Initialize' functions.
//
// For Vulkan, the same set of features can be controlled through the
// 'VkDeviceDiagnosticsConfigFlagBitsNV' defined by the
// 'VK_NV_device_diagnostics_config' extension. See also the description of the
// configuration flag bits in the 'Readme.md' file.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_AFTERMATH_DECLARE_ENUM(FeatureFlags)
{
    // The minimal flag only allows use of the 'GFSDK_Aftermath_GetDeviceStatus'
    // entry point and GPU crash dump generation with basic information about the
    // GPU fault.
    GFSDK_Aftermath_FeatureFlags_Minimum = 0x00000000,

    // This flag enables support for DX Aftermath event markers, including both
    // the support for user markers that are explicitly added by the application
    // via 'GFSDK_Aftermath_SetEventMarker' and automatic call stack markers
    // controlled by 'GFSDK_Aftermath_FeatureFlags_CallStackCapturing'.
    //
    // For Vulkan, the event marker (checkpoints) feature is enabled through the
    // 'VK_NV_device_diagnostic_checkpoints' extension.
    //
    // NOTE: Using event markers should be considered carefully as they can cause
    // very high CPU overhead when used in high frequency code paths. Due to the
    // inherent overhead, event markers should be used only for debugging purposes on
    // development or QA systems. Therefore, on some driver versions, Aftermath
    // event marker tracking on DX11 and DX12 is only available if the Nsight
    // Aftermath GPU Crash Dump Monitor is running on the system. This requirement
    // applies to R495 to R530 drivers for DX12 and R495+ drivers for DX11. No Aftermath
    // configuration needs to be made in the Monitor. It serves only as a dongle to
    // ensure Aftermath event markers do not impact application performance on end
    // user systems. That means this flag will be ignored if the monitor process is
    // not detected.
    GFSDK_Aftermath_FeatureFlags_EnableMarkers = 0x00000001,

    // With this flag set, live and recently destroyed resources are tracked by the
    // display driver. In case of a page fault that information will be used to
    // identify possible candidates of deleted resources that correspond to the fault
    // address. Information about the most likely resource related to the fault will
    // be included in the page fault data, including, for example, information about
    // the size of the resource, its format, and the epoch time stamp when it was
    // deleted.
    //
    // The corresponding feature configuration flag for Vulkan is
    // 'VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV'.
    //
    // NOTE: Enabling this feature will incur memory overhead due to the additional
    // tracking data managed by the display driver as well as CPU overhead for each
    // resource creation and destruction.
    GFSDK_Aftermath_FeatureFlags_EnableResourceTracking = 0x00000002,

    // With this flag set, event markers are automatically set for all draw calls,
    // compute dispatches and copy operations to capture the CPU call stack for the
    // corresponding API call as the event marker payload.
    //
    // The corresponding feature configuration flag for Vulkan is
    // 'VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV'.
    //
    // NOTE: Requires also 'GFSDK_Aftermath_FeatureFlags_EnableMarkers' to be set.
    //
    // NOTE: Enabling this feature will cause very high CPU overhead during command
    // list recording. Due to the inherent overhead, call stack capturing should only
    // be used for debugging purposes on development or QA systems and should not be
    // enabled in applications shipped to customers. Therefore, on R495+ drivers,
    // call stack capturing on DX11 and DX12 is only available if the Nsight Aftermath
    // GPU Crash Dump Monitor is running on the system. No Aftermath configuration
    // needs to be made in the Monitor. It serves only as a dongle to ensure call
    // stack capturing does not impact application performance on end user systems.
    // That means this flag will be ignored if the monitor process is not detected.
    //
    // NOTE: When enabling this feature, Aftermath GPU crash dumps will include file
    // paths to the crashing application's executable as well as all DLLs it has loaded.
    GFSDK_Aftermath_FeatureFlags_CallStackCapturing = 0x40000000,

    // With this flag set, debug information (line tables for mapping from the shader
    // IL passed to the driver to the shader microcode) for all shaders is generated
    // by the display driver.
    //
    // The corresponding feature configuration flag for Vulkan is
    // 'VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV'.
    //
    // NOTE: Using this feature should be considered carefully. It may cause
    // considerable shader compilation overhead and additional overhead for handling
    // the corresponding shader debug information callbacks (if provided to
    // 'GFSDK_Aftermath_EnableGpuCrashDumps').
    //
    // NOTE: shader debug information is only supported for DX12 applications using
    // shaders compiled as DXIL. This flag has no effect on DX11 applications.
    GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo = 0x00000008,

    // If this flag is set, the GPU will run in a mode that allows to capture runtime
    // errors in shaders that are not caught with default driver settings. This may
    // provide additional information for debugging GPU hangs, GPU crashes or other
    // unexpected behavior related to shader execution.
    //
    // The corresponding feature configuration flag for Vulkan is
    // 'VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV'.
    //
    // NOTE: Enabling this feature does not cause any performance overhead, but it
    // may result in additional crash dumps being generated to report issues in
    // shaders that exhibit undefined behavior or have hidden bugs, which so far went
    // unnoticed, because with default driver settings the HW silently ignores them.
    //
    // NOTE: This feature is only supported on R515 or later drivers. The feature
    // flag will be ignored on earlier driver versions.
    //
    // Examples for problems that are caught when this feature is enabled:
    //
    // o) Accessing memory using misaligned addresses, such as reading or
    //    writing a byte address that is not a multiple of the access size.
    //
    // o) Accessing memory out-of-bounds, such as reading or writing beyond the
    //    declared bounds of (group) shared or thread local memory or reading from an
    //    out-of-bounds constant buffer address.
    //
    // o) Hitting call stack limits.
    GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting = 0x00000010,
};

#if defined(__d3d11_h__) || defined(__d3d12_h__)

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_ContextHandle
// ---------------------------------
//
// Used with Aftermath entry points to reference an API object.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_AFTERMATH_DECLARE_HANDLE(GFSDK_Aftermath_ContextHandle);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_ResourceHandle
// ---------------------------------
//
// Used with the 'GFSDK_Aftermath_DX12_RegisterResource' and
// 'GFSDK_Aftermath_DX12_UnregisterResource' entry points to reference an API
// resource object.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_AFTERMATH_DECLARE_HANDLE(GFSDK_Aftermath_ResourceHandle);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_ContextData
// ---------------------------------
//
//      DEPRECATED - this functionality will go away in a future release. Do not use!
//
// Used with, 'GFSDK_Aftermath_GetData'. Filled with information, about each
// requested context.
//
/////////////////////////////////////////////////////////////////////////
typedef struct GFSDK_Aftermath_ContextData
{
    GFSDK_AFTERMATH_DECLARE_POINTER_MEMBER(void*, markerData);
    uint32_t markerSize;
    GFSDK_Aftermath_Context_Status status;
} GFSDK_Aftermath_ContextData;

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_ResourceDescriptor
// ---------------------------------
//
//      DEPRECATED - this functionality will go away in a future release. Do not use!
//
// Minimal description of a graphics resource.
//
/////////////////////////////////////////////////////////////////////////
typedef struct GFSDK_Aftermath_ResourceDescriptor
{
    // This is available in DX12 only and only if the application registers the
    // resource pointers using 'GFSDK_Aftermath_DX12_RegisterResource'.
#ifdef __d3d12_h__
    GFSDK_AFTERMATH_DECLARE_POINTER_MEMBER(ID3D12Resource*, pAppResource);
#else
    GFSDK_AFTERMATH_DECLARE_POINTER_MEMBER(void*, pAppResource);
#endif

    uint64_t size;

    uint32_t width;
    uint32_t height;
    uint32_t depth;

    uint32_t mipLevels;

    uint32_t format; // DXGI_FORMAT

    GFSDK_AFTERMATH_DECLARE_BOOLEAN_MEMBER(bIsBufferHeap);
    GFSDK_AFTERMATH_DECLARE_BOOLEAN_MEMBER(bIsStaticTextureHeap);
    GFSDK_AFTERMATH_DECLARE_BOOLEAN_MEMBER(bIsRtvDsvTextureHeap);
    GFSDK_AFTERMATH_DECLARE_BOOLEAN_MEMBER(bPlacedResource);

    GFSDK_AFTERMATH_DECLARE_BOOLEAN_MEMBER(bWasDestroyed);
} GFSDK_Aftermath_ResourceDescriptor;

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_PageFaultInformation
// ---------------------------------
//
//      DEPRECATED - this functionality will go away in a future release. Do not use!
//
// Used with GFSDK_Aftermath_GetPageFaultInformation
//
/////////////////////////////////////////////////////////////////////////
typedef struct GFSDK_Aftermath_PageFaultInformation
{
    uint64_t faultingGpuVA;
    GFSDK_Aftermath_ResourceDescriptor resourceDesc;
    GFSDK_AFTERMATH_DECLARE_BOOLEAN_MEMBER(bHasPageFaultOccured);
} GFSDK_Aftermath_PageFaultInformation;

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_DX11_Initialize
// GFSDK_Aftermath_DX12_Initialize
// ---------------------------------
//
// [pDx11Device]; DX11-Only
//      The current dx11 device pointer.
//
// [pDx12Device]; DX12-Only
//      The current dx12 device pointer.
//
// flags;
//      The set of features to enable when initializing Aftermath.
//
// version;
//      Must be set to 'GFSDK_Aftermath_Version_API'. Used for checking against library
//      version.
//
//// DESCRIPTION;
//      Library must be initialized before any other call is made. This should be
//      done after device creation. Aftermath currently only supports one D3D
//      device, the first one that is initialized.
//
/////////////////////////////////////////////////////////////////////////
#ifdef __d3d11_h__
GFSDK_Aftermath_API GFSDK_Aftermath_DX11_Initialize(GFSDK_Aftermath_Version version, uint32_t flags, ID3D11Device* const pDx11Device);
#endif
#ifdef __d3d12_h__
GFSDK_Aftermath_API GFSDK_Aftermath_DX12_Initialize(GFSDK_Aftermath_Version version, uint32_t flags, ID3D12Device* const pDx12Device);
#endif

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_DX11_CreateContextHandle
// GFSDK_Aftermath_DX12_CreateContextHandle
// ---------------------------------
//
// (pDx11DeviceContext); DX11-Only
//      Device context to use with Aftermath.
//
// (pDx12Unknown); DX12-Only
//      Command list, Command Queue, or Device to use with Aftermath. If a device,
//      must be the same device given to 'GFSDK_Aftermath_DX12_Initialize'. If a
//      command list, it must be in the recording state.
//
// pOutContextHandle;
//      The context handle for the specified context/command list/command
//      queue/device to be used with future Aftermath calls.
//
//// DESCRIPTION;
//      Before Aftermath event markers can be inserted into the command stream of a
//      DX12 command list or a DX11 device context, a context handle must first be
//      fetched. A context handle is also required for querying the event marker
//      status with 'GFSDK_Aftermath_GetData'.
//
/////////////////////////////////////////////////////////////////////////
#ifdef __d3d11_h__
GFSDK_Aftermath_API GFSDK_Aftermath_DX11_CreateContextHandle(ID3D11DeviceContext* const pDx11DeviceContext, GFSDK_Aftermath_ContextHandle* pOutContextHandle);
#endif
#ifdef __d3d12_h__
GFSDK_Aftermath_API GFSDK_Aftermath_DX12_CreateContextHandle(IUnknown* const pDx12Unknown, GFSDK_Aftermath_ContextHandle* pOutContextHandle);
#endif

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_API GFSDK_Aftermath_ReleaseContextHandle
// -------------------------------------
//
// contextHandle;
//      Context to release
//
// DESCRIPTION;
//      Cleans up any resources associated with an Aftermath context.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_ReleaseContextHandle(const GFSDK_Aftermath_ContextHandle contextHandle);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_SetEventMarker
// -------------------------------------
//
// contextHandle;
//      Command list currently being populated, which must be in the recording state.
//
// markerData;
//      Pointer to data used for event marker.
//
//      NOTE: If 'markerDataSize' is also provided, an internal copy will be made of this
//      data. In that case there is no need to keep it around after this call - stack
//      allocation of the data is safe.
//
// markerDataSize;
//      Size of event marker data in bytes.
//
//      NOTE: Passing a 0 for this parameter is valid, and will instruct Aftermath to
//      only copy off the pointer supplied by 'markerData', rather than internally
//      making a copy. In this case, additional work is required to include the
//      marker data into Aftermath crash dumps. The application needs to keep track
//      of the 'markerData' pointer and resolve it to the actual marker data via the
//      'resolveMarkerCb' provided to 'GFSDK_Aftermath_EnableGpuCrashDumps'.
//
//      NOTE: Aftermath will internally truncate marker data to a maximum size of
//      1024 bytes. Use 'markerDataSize = 0' and manually manage memory for markers if
//      the application requires larger ones.
//
// DESCRIPTION;
//      Drops an event into the command stream with a payload that can be linked back
//      to the data given here, 'markerData'. It's safe to call from multiple threads
//      simultaneously, normal D3D API threading restrictions apply.
//
//      NOTE: Using event markers should be considered carefully as they can cause
//      considerable CPU overhead when used in high frequency code paths.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_SetEventMarker(const GFSDK_Aftermath_ContextHandle contextHandle, const void* pMarkerData, const uint32_t markerDataSize);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GetData
// ------------------------------
//
//      DEPRECATED - this functionality will go away in a future release. Do not use!
//
// numContexts;
//      Number of contexts to fetch information for.
//
// pContextHandles;
//      Array of contexts containing Aftermath event markers.
//
// pOutContextData;
//      OUTPUT: context data for each context requested. Contains event last reached
//      on the GPU, and status of context if applicable (DX12-Only).
//
//      NOTE: must allocate enough space for 'numContexts' worth of structures.
//      Stack allocation is fine.
//
// DESCRIPTION;
//      Once a TDR/crash/hang has occurred (or whenever you like), call this API to
//      retrieve the events last processed by the GPU on the given contexts. The
//      context handles may refer to DX12 command lists, queues, or DX11 device
//      contexts.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_GetData(const uint32_t numContexts, const GFSDK_Aftermath_ContextHandle* pContextHandles, GFSDK_Aftermath_ContextData* pOutContextData);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GetContextError
// ------------------------------
//
//      DEPRECATED - this functionality will go away in a future release. Do not use!
//
// pContextData;
//      Context data for which to determine error status.
//
// DESCRIPTION;
//      Call this to determine the detailed failure reason for
//      GFSDK_Aftermath_ContextData with 'status == GFSDK_Aftermath_Context_Status_Invalid'.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_GetContextError(const GFSDK_Aftermath_ContextData* pContextData);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GetDeviceStatus
// ---------------------------------
//
// pOutStatus;
//      OUTPUT: Device status.
//
//// DESCRIPTION;
//      Return the status of a D3D device. See 'GFSDK_Aftermath_Device_Status'.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_GetDeviceStatus(GFSDK_Aftermath_Device_Status* pOutStatus);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GetPageFaultInformation
// ---------------------------------
//
//      DEPRECATED - this functionality will go away in a future release. Do not use!
//
// pOutPageFaultInformation;
//      OUTPUT: Information about a page fault which may have occurred.
//
//// DESCRIPTION;
//      Return any information available about a recent page fault which may have
//      occurred, causing a device removed scenario. See
//      'GFSDK_Aftermath_PageFaultInformation'.
//
//      Requires WDDMv2 (Windows 10) or later
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_GetPageFaultInformation(GFSDK_Aftermath_PageFaultInformation* pOutPageFaultInformation);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_DX12_RegisterResource
// ---------------------------------
//
// pResource;
//      ID3D12Resource to register.
//
// pOutResourceHandle;
//      OUTPUT: Aftermath resource handle for the resource that was registered.
//
//// DESCRIPTION;
//      Registers an 'ID3D12Resource' with Aftermath. This allows Aftermath to map
//      the GPU virtual address of a page fault to the corresponding 'ID3D12Resource'
//      pointer and the driver level resource tracking data (enabled via
//      'GFSDK_Aftermath_FeatureFlags_EnableResourceTracking'). If called after
//      'ID3D12Object::SetName', it will also allow tracking of the debug object name
//      assigned to the resource (requires R530 driver).
//
//      NOTE: This function is only supported on Windows 10 RS4 and later. It will
//      return 'GFSDK_Aftermath_Result_FAIL_D3dDllNotSupported', if the version of the
//      D3D DLL loaded by the application is not supported.
//
//      NOTE: This function is not compatible with graphics debuggers, such as Nsight
//      Graphics, PIX, or the Visual Studio Graphics Debugger. It may fail with
//      'GFSDK_Aftermath_Result_FAIL_D3dDllInterceptionNotSupported' when called, if
//      such a debugger is active.
//
//      NOTE: This is a BETA FEATURE and may not work with all versions of Windows.
//
/////////////////////////////////////////////////////////////////////////
#if defined(__d3d12_h__)
GFSDK_Aftermath_API GFSDK_Aftermath_DX12_RegisterResource(ID3D12Resource* const pResource, GFSDK_Aftermath_ResourceHandle* pOutResourceHandle);
#endif

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_DX12_UnregisterResource
// ---------------------------------
//
// resourceHandle;
//      Aftermath resource handle for a resource that was registered earlier with
//      'GFSDK_Aftermath_DX12_RegisterResource'.
//
//// DESCRIPTION;
//      Unregisters a previously registered resource. This will tell Aftermath
//      that the resource tracking data that was allocated for this resource
//      may be reused in LRU order for new resources being registered via
//      'GFSDK_Aftermath_DX12_RegisterResource'. Aftermath will guarantee a
//      look-back of the least recently unregistered 1024 resources before
//      reusing the corresponding tracking data.
//
//      NOTE: This is a BETA FEATURE and may not work with all versions of Windows.
//
/////////////////////////////////////////////////////////////////////////
#if defined(__d3d12_h__)
GFSDK_Aftermath_API GFSDK_Aftermath_DX12_UnregisterResource(const GFSDK_Aftermath_ResourceHandle resourceHandle);
#endif

/////////////////////////////////////////////////////////////////////////
//
// Function pointer definitions - if dynamic loading is preferred.
//
/////////////////////////////////////////////////////////////////////////
#if defined(__d3d11_h__)
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_DX11_Initialize)(GFSDK_Aftermath_Version version, uint32_t flags, ID3D11Device* const pDx11Device);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_DX11_CreateContextHandle)(ID3D11DeviceContext* const pDx11DeviceContext, GFSDK_Aftermath_ContextHandle* pOutContextHandle);
#endif

#if defined(__d3d12_h__)
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_DX12_Initialize)(GFSDK_Aftermath_Version version, uint32_t flags, ID3D12Device* const pDx12Device);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_DX12_CreateContextHandle)(IUnknown* const pDx12CommandList, GFSDK_Aftermath_ContextHandle* pOutContextHandle);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_DX12_RegisterResource)(ID3D12Resource* const pResource, GFSDK_Aftermath_ResourceHandle* pOutResourceHandle);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_DX12_UnregisterResource)(const GFSDK_Aftermath_ResourceHandle resourceHandle);
#endif

#if defined(__d3d11_h__) || defined(__d3d12_h__)
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_ReleaseContextHandle)(const GFSDK_Aftermath_ContextHandle contextHandle);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_SetEventMarker)(const GFSDK_Aftermath_ContextHandle contextHandle, const void* markerData, const uint32_t markerDataSize);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_GetData)(const uint32_t numContexts, const GFSDK_Aftermath_ContextHandle* ppContextHandles, GFSDK_Aftermath_ContextData* pOutContextData);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_GetContextError)(const GFSDK_Aftermath_ContextData* pContextData);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_GetDeviceStatus)(GFSDK_Aftermath_Device_Status* pOutStatus);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_GetPageFaultInformation)(GFSDK_Aftermath_PageFaultInformation* pOutPageFaultInformation);
#endif

#endif // defined(__d3d11_h__) || defined(__d3d12_h__)

#if defined(VULKAN_H_)
// See VK_NV_device_diagnostics_config
// See VK_NV_device_diagnostic_checkpoints
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#pragma pack(pop)

#endif // GFSDK_Aftermath_H
