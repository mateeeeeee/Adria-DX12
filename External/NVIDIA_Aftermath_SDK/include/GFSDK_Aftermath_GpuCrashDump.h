/*
* Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
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
*  HOW TO USE AFTERMATH GPU CRASH DUMP COLLECTION:
*  -----------------------------------------------
*
*  1)  Call 'GFSDK_Aftermath_EnableGpuCrashDumps', to enable GPU crash dump collection.
*      This must be done before any other library calls are made and before any D3D
*      device is created by the application.
*
*      With this call the application can register a callback function that is invoked
*      with the GPU crash dump data once a TDR/hang occurs. In addition, it is also
*      possible to provide optional callback functions for collecting shader debug
*      information and for providing additional descriptive data from the application to
*      include in the crash dump.
*
*      Enabling GPU crash dumps will also override any settings from an also active
*      Nsight Graphics GPU crash dump monitor for the calling process.
*
*
*  2)  On DX11/DX12, call 'GFSDK_Aftermath_DXxx_Initialize', to initialize the library and
*      to enable additional Aftermath features that will affect the data captured in
*      the GPU crash dumps, such as Aftermath event markers, automatic call stack
*      markers, resource tracking, shader debug information, or additional shader
*      error reporting. See 'GFSDK_Aftermath.h' for more details.
*
*      On Vulkan use the 'VK_NV_device_diagnostics_config' extension to enable
*      additional Aftermath features, such as automatic call stack markers, resource
*      tracking, shader debug information, or additional shader error reporting. See
*      'Readme.md' for more details.
*
*
*  4)  Before the application shuts down, call 'GFSDK_Aftermath_DisableGpuCrashDumps' to
*      disable GPU crash dump collection.
*
*      Disabling GPU crash dumps will also re-establish any settings from an also active
*      Nsight Graphics GPU crash dump monitor for the calling process.
*
*
*  5)  If the application detects a potential GPU crash, i.e., device removed/lost,
*      call 'GFSDK_Aftermath_GetCrashDumpStatus' to check the GPU crash dump status.
*      The application should then wait until Aftermath has finished processing the
*      crash dump before releasing the device or exiting.
*
*      The recommended process for handling device removed/lost events is as follows:
*
*           a) Call 'GFSDK_Aftermath_GetCrashDumpStatus' to check the GPU crash dump status.
*
*           b) If the status is "Unknown", this means the graphics driver does not support
*              the crash dump status query feature. But it is still possible to receive the
*              "Finished" or "Failed" status. The application should continue to poll the
*              status as described in step d) below.
*
*           c) If the status is not "Unknown" and not "NotStarted", this means Aftermath
*              has detected the GPU crash.
*
*           d) The application should wait for a few seconds to allow the Aftermath
*              graphics driver thread to collect the GPU crash dump data. Start polling the
*              status until the crash dump data has been collected and the notification
*              callback has been processed by the application, or a timeout of a couple of
*              seconds has expired.
*
*           e) If the timeout expires or the status returns "Finished" or "Failed" you
*              should continue handling the device lost event as normal (e.g., release
*              the device and/or terminate the application).
*
*      Pseudo code implementing the above steps:
*
*          if (deviceLost)
*          {
*              // Check Aftermath crash dump status
*              GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
*              AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetCrashDumpStatus(&status));
*
*              // Loop while Aftermath crash dump data collection has not finished or
*              // the application is still processing the crash dump data.
*              while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
*                     status != GFSDK_Aftermath_CrashDump_Status_Finished &&
*                     !timeout)
*              {
*                  // Wait for a couple of milliseconds, and poll the crash dump status again.
*                  Sleep(50);
*                  GFSDK_Aftermath_GetCrashDumpStatus(&status);
*              }
*
*              HandleDeviceLost();
*          }
*
*
*  OPTIONAL:
*
*  o)  (Optional) Instrument the application with event markers as described in
*      'GFSDK_Aftermath.h'.
*
*  o)  (Optional, DX12-Only) Register DX12 resource pointers with Aftermath as
*      described in 'GFSDK_Aftermath.h'.
*
*
*  PERFORMANCE TIPS:
*
*  o)  Enabling shader debug information creation will introduce shader compile
*      time overhead as well as memory overhead for handling the debug information.
*
*  o)  User event markers cause considerable overhead and should be used very
*      carefully.
*
*  o)  Automatic call stack markers for draw calls, compute and ray tracing
*      dispatches, acceleration structure building, and copy operations provide a
*      less intrusive alternative to manually injecting event markers for every
*      command. However, they are in general even more expensive in terms of CPU
*      overhead and should be avoided in shipping applications.
*
*/

#ifndef GFSDK_Aftermath_GpuCrashDump_H
#define GFSDK_Aftermath_GpuCrashDump_H

#include "GFSDK_Aftermath_Defines.h"

#pragma pack(push, 8)

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags
// ---------------------------------
//
// Flags to configure for which graphics APIs to enable GPU crash dumps.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_AFTERMATH_DECLARE_ENUM(GpuCrashDumpWatchedApiFlags)
{
    // Default setting - GPU crash dump tracking disabled.
    GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_None = 0x0,

    // Enable GPU crash dump tracking for the DX API.
    GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX = 0x1,

    // Enable GPU crash dump tracking for the Vulkan API.
    GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan = 0x2,
};

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GpuCrashDumpFeatureFlags
// ---------------------------------
//
// Flags to configure GPU crash dump-specific Aftermath features.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_AFTERMATH_DECLARE_ENUM(GpuCrashDumpFeatureFlags)
{
    // Default settings
    GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default = 0x0,

    // Defer shader debug information callbacks until an actual GPU crash dump
    // is generated and execute shader debug information callbacks only for the
    // shaders related to the crash dump.
    //
    // NOTE: Using this option will increase the memory footprint of the
    // application.
    GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks = 0x1,
};

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GpuCrashDumpDescriptionKey
// ---------------------------------
//
// Key definitions for user-defined GPU crash dump description.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_AFTERMATH_DECLARE_ENUM(GpuCrashDumpDescriptionKey)
{
    // Predefined key for application name.
    GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName = 0x1,

    // Predefined key for application version.
    GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion = 0x2,

    // Base key for creating user-defined key-value pairs. Any value greater or equal
    // to 'GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined' will create a
    // user-defined key-value pair.
    GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined = 0x10000,
};

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_CrashDump_Status
// ---------------------------------
//
// Aftermath GPU crash dump generation progress status.
//
// Applications are expected to check the status of the Aftermath crash dump
// generation progress if they detect a potential GPU crash, i.e., device
// removed/lost. For more details, see the description on how to set up an application
// for GPU crash dump collection at the beginning of this file.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_AFTERMATH_DECLARE_ENUM(CrashDump_Status)
{
    // No GPU crash has been detected by Aftermath, so far.
    GFSDK_Aftermath_CrashDump_Status_NotStarted = 0,

    // A GPU crash happened and was detected by Aftermath. Aftermath started to
    // collect crash dump data.
    GFSDK_Aftermath_CrashDump_Status_CollectingData,

    // Aftermath failed to collect crash dump data. No further callback will be
    // invoked.
    GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed,

    // Aftermath is invoking the 'gpuCrashDumpCb' callback after collecting the crash
    // dump data successfully.
    GFSDK_Aftermath_CrashDump_Status_InvokingCallback,

    // The 'gpuCrashDumpCb' callback returned and Aftermath finished processing the
    // GPU crash. The application should now continue with handling the device
    // removed/lost situation.
    GFSDK_Aftermath_CrashDump_Status_Finished,

    // Unknown problem - likely using an older driver incompatible with this
    // Aftermath feature.
    GFSDK_Aftermath_CrashDump_Status_Unknown,
};

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_AddGpuCrashDumpDescription
// ---------------------------------
//
// Function for adding user-defined description key-value pairs used by
// 'GFSDK_Aftermath_GpuCrashDumpDescriptionCb'.
//
// Key must be one of the predefined keys of
// 'GFSDK_Aftermath_GpuCrashDumpDescriptionKey' or a user-defined key based on
// 'GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined'. All keys greater than
// the last predefined key in 'GFSDK_Aftermath_GpuCrashDumpDescriptionKey' and
// smaller than 'GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined' are
// considered illegal and ignored.
//
/////////////////////////////////////////////////////////////////////////
typedef void (GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription)(uint32_t key, const char* value);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GpuCrashDumpCb
// ---------------------------------
//
// GPU crash dump callback.
//
// If registered via 'GFSDK_Aftermath_EnableGpuCrashDumps' it will be called with the
// crash dump data when a GPU crash is detected by Aftermath. See the description of
// 'GFSDK_Aftermath_EnableGpuCrashDumps' for more details.
//
// NOTE: Except for the 'pUserData' pointer, all pointer values passed to the
// callbacks are only valid for the duration of the call! An implementation
// must make copies of the data if it intends to store it beyond that.
//
/////////////////////////////////////////////////////////////////////////
typedef void (GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_GpuCrashDumpCb)(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_ShaderDebugInfoCb
// ---------------------------------
//
// Shader debug information callback.
//
// If registered via 'GFSDK_Aftermath_EnableGpuCrashDumps' it will be called with
// shader debug information (line tables for mapping from the shader IL passed to the
// driver to the shader microcode) if the shader debug information generation feature
// is enabled: 'GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo' or
// 'VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV'. Also see the
// description of 'GFSDK_Aftermath_EnableGpuCrashDumps' for more details.
//
// NOTE: Except for the 'pUserData' pointer, all pointer values passed to the
// callbacks are only valid for the duration of the call! An implementation
// must make copies of the data if it intends to store it beyond that.
//
/////////////////////////////////////////////////////////////////////////
typedef void (GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_ShaderDebugInfoCb)(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize, void* pUserData);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GpuCrashDumpDescriptionCb
// ---------------------------------
//
// Crash dump description callback.
//
// If registered via 'GFSDK_Aftermath_EnableGpuCrashDumps' it will be called during
// GPU crash dump generation, i.e., before 'GFSDK_Aftermath_GpuCrashDumpCb' is called,
// and allows the application to provide additional information to be captured in the
// crash dump by calling the provided 'addValue' function. See the description of
// 'GFSDK_Aftermath_EnableGpuCrashDumps' for more details.
//
/////////////////////////////////////////////////////////////////////////
typedef void (GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_GpuCrashDumpDescriptionCb)(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addValue, void* pUserData);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_ResolveMarkerCb
// ---------------------------------
//
// Marker data resolution callback.
//
// If registered via 'GFSDK_Aftermath_EnableGpuCrashDumps' it will be called during
// GPU crash dump data generation, i.e., before 'GFSDK_Aftermath_GpuCrashDumpCb' is
// called, when a DX event marker or a Vulkan checkpoint will be recorded into the
// crash dump. See the description of 'GFSDK_Aftermath_EnableGpuCrashDumps' for more details.
//
// NOTE: Except for the 'pUserData' pointer, all pointer values passed to the
// callbacks are only valid for the duration of the call! The application must ensure
// that the pointer returned through 'ppResolvedMarkerData' is valid after returning
// from the callback. Then, the GPU crash dump data collection process will make an
// internal copy of it. So, it's safe to reuse the memory in the next call, i.e., it's
// OK to store the memory blob where the 'ppResolvedMarkerData' is pointed to in static memory.
//
/////////////////////////////////////////////////////////////////////////
typedef void (GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_ResolveMarkerCb)(const void* pMarkerData, const uint32_t markerDataSize, void* pUserData, void** ppResolvedMarkerData, uint32_t* pResolvedMarkerDataSize);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_EnableGpuCrashDumps
// ---------------------------------
//
// apiVersion;
//      Must be set to 'GFSDK_Aftermath_Version_API'. Used for checking against
//      library version.
//
// watchedApis;
//      Controls which graphics APIs to watch for crashes. A combination of
//      'GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags'.
//
// flags;
//      Controls GPU crash dump specific behavior. A combination of
//      'GFSDK_Aftermath_GpuCrashDumpFeatureFlags'.
//
// gpuCrashDumpCb;
//      Callback function to be called when new GPU crash dump data is available.
//
//      NOTE: This callback is free-threaded, ensure the provided function is
//      thread-safe.
//
// shaderDebugInfoCb;
//      Optional, can be NULL.
//
//      Callback function to be called when new shader debug information data is
//      available. Shader debug information generation needs to be enabled by
//      setting the corresponding feature flags:
//      * For DX: 'GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo'
//      * For Vulkan: 'VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV'
//
//      NOTE: Shader debug information is only supported for DX12 (DXIL) and Vulkan
//      (SPIR-V) shaders.
//
//      NOTE: If not using 'GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks',
//      'shaderDebugInfoCb' will be invoked for every shader compilation by the
//      graphics driver, even if there will be never an invocation of 'gpuCrashDumpCb'.
//
//      NOTE: This callback is free-threaded, ensure the provided function is
//      thread-safe.
//
// descriptionCb;
//      Optional, can be NULL.
//
//      Callback function that allows the application to provide additional
//      descriptive values to be include in crash dumps. This will be called before
//      'gpuCrashDumpCb'.
//
//      NOTE: This callback is free-threaded, ensure the provided function is
//      thread-safe.
//
// resolveMarkerCb;
//      Optional, can be NULL.
//
//      Callback function to be called when the crash dump data generation encounters
//      an event marker with a size of zero. This means that
//      'GFSDK_Aftermath_SetEventMarker' was called with 'markerDataSize = 0', meaning
//      that the marker payload itself is managed by the application rather than
//      copied by Aftermath internally. All Vulkan markers set using the
//      'NV_device_diagnostic_checkpoints' extension are application-managed as well.
//      This callback allows the application to pass the marker's associated data
//      back to the crash dump generation process to be included in the crash dump
//      data. The application should set the value of 'ppResolvedMarkerData' to the
//      pointer of the marker's data, and set the value of 'markerSize' to the size
//      of the marker's data in bytes.
//
//      NOTE: Applications must ensure that the marker data memory passed back via
//      'ppResolvedMarkerData' will remain valid for the entirety of the crash dump
//      generation process, i.e., until 'gpuCrashDumpCb' is called.
//
//      NOTE: This callback is only supported on R495 or later NVIDIA graphics drivers. If
//      the application is running on a system using an earlier driver version, it will
//      be ignored.
//
//      NOTE: This callback is free-threaded, ensure the provided function is
//      thread-safe.
//
// pUserData;
//      Optional, can be NULL.
//
//      User data pointer passed to the callbacks.
//
//// DESCRIPTION;
//      Device independent initialization call to enable Aftermath GPU crash dump
//      creation. This function must be called before any D3D or Vulkan device is
//      created by the application.
//
//      NOTE: This overrides any settings from an also active GPU crash dump monitor
//      for this process!
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_EnableGpuCrashDumps(
    GFSDK_Aftermath_Version apiVersion,
    uint32_t watchedApis,
    uint32_t flags,
    PFN_GFSDK_Aftermath_GpuCrashDumpCb gpuCrashDumpCb,
    PFN_GFSDK_Aftermath_ShaderDebugInfoCb shaderDebugInfoCb,
    PFN_GFSDK_Aftermath_GpuCrashDumpDescriptionCb descriptionCb,
    PFN_GFSDK_Aftermath_ResolveMarkerCb resolveMarkerCb,
    void* pUserData);

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_DisableGpuCrashDumps
// ---------------------------------
//
//// DESCRIPTION;
//      Device independent call to disable Aftermath GPU crash dump creation.
//      Re-enables settings from an also active GPU crash dump monitor for the
//      current process!
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_DisableGpuCrashDumps();

/////////////////////////////////////////////////////////////////////////
// GFSDK_Aftermath_GetCrashDumpStatus
// ---------------------------------
//
// pOutStatus;
//      OUTPUT: Crash dump status.
//
//// DESCRIPTION;
//      If the application detects a potential crash  (i.e., device
//      removed/lost event), applications are expected to check the status of
//      the Aftermath crash dump generation progress. This function allows to
//      query the status of the GPU crash detection and the crash dump data
//      collection. For more details, see 'GFSDK_Aftermath_CrashDump_Status'
//      and the description of how to set up an application for GPU crash dump
//      collection at the beginning of this file.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_API GFSDK_Aftermath_GetCrashDumpStatus(GFSDK_Aftermath_CrashDump_Status* pOutStatus);

/////////////////////////////////////////////////////////////////////////
//
// Function pointer definitions - if dynamic loading is preferred.
//
/////////////////////////////////////////////////////////////////////////
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_EnableGpuCrashDumps)(GFSDK_Aftermath_Version apiVersion, uint32_t watchedApis, uint32_t flags, PFN_GFSDK_Aftermath_GpuCrashDumpCb gpuCrashDumpCb, PFN_GFSDK_Aftermath_ShaderDebugInfoCb shaderDebugInfoCb, PFN_GFSDK_Aftermath_GpuCrashDumpDescriptionCb descriptionCb, PFN_GFSDK_Aftermath_ResolveMarkerCb resolveMarkerCb, void* pUserData);
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_DisableGpuCrashDumps)();
GFSDK_Aftermath_PFN(GFSDK_AFTERMATH_CALL *PFN_GFSDK_Aftermath_GetCrashDumpStatus)(GFSDK_Aftermath_CrashDump_Status* pOutStatus);

#ifdef __cplusplus
} // extern "C"
#endif

#pragma pack(pop)

#endif // GFSDK_Aftermath_GpuCrashDump_H
