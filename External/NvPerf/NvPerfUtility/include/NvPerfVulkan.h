/*
* Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include <vulkan/vulkan.h>
#include "NvPerfInit.h"
#include "NvPerfDeviceProperties.h"
#include "NvPerfPeriodicSamplerGpu.h"
#include "nvperf_vulkan_host.h"
#include "nvperf_vulkan_target.h"

namespace nv { namespace perf {

    //
    // Vulkan Only Utilities
    //

    struct VulkanFunctions
    {
        PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr;

        // Vulkan instance functions
        PFN_vkGetPhysicalDeviceMemoryProperties pfnVkGetPhysicalDeviceMemoryProperties;
        PFN_vkGetPhysicalDeviceProperties pfnVkGetPhysicalDeviceProperties;

        // Vulkan device functions
        PFN_vkAllocateCommandBuffers pfnVkAllocateCommandBuffers;
        PFN_vkAllocateMemory pfnVkAllocateMemory;
        PFN_vkBeginCommandBuffer pfnVkBeginCommandBuffer;
        PFN_vkBindBufferMemory pfnVkBindBufferMemory;
        PFN_vkCreateBuffer pfnVkCreateBuffer;
        PFN_vkCreateCommandPool pfnVkCreateCommandPool;
        PFN_vkCreateFence pfnVkCreateFence;
        PFN_vkDestroyBuffer pfnVkDestroyBuffer;
        PFN_vkDestroyCommandPool pfnVkDestroyCommandPool;
        PFN_vkDestroyFence pfnVkDestroyFence;
        PFN_vkEndCommandBuffer pfnVkEndCommandBuffer;
        PFN_vkFreeCommandBuffers pfnVkFreeCommandBuffers;
        PFN_vkFreeMemory pfnVkFreeMemory;
        PFN_vkGetBufferMemoryRequirements pfnVkGetBufferMemoryRequirements;
        PFN_vkGetFenceStatus pfnVkGetFenceStatus;
        PFN_vkMapMemory pfnVkMapMemory;
        PFN_vkQueueSubmit pfnVkQueueSubmit;
        PFN_vkQueueWaitIdle pfnVkQueueWaitIdle;
        PFN_vkResetCommandBuffer pfnVkResetCommandBuffer;
        PFN_vkResetFences pfnVkResetFences;
        PFN_vkUnmapMemory pfnVkUnmapMemory;
        PFN_vkWaitForFences pfnVkWaitForFences;
        PFN_vkCreateQueryPool pfnVkCreateQueryPool;
        PFN_vkDestroyQueryPool pfnVkDestroyQueryPool;
        PFN_vkCmdWriteTimestamp pfnVkCmdWriteTimestamp;
        PFN_vkCmdResetQueryPool pfnVkCmdResetQueryPool;
        PFN_vkGetQueryPoolResults pfnVkGetQueryPoolResults;

#if defined(VK_NO_PROTOTYPES)
        void Initialize(VkInstance instance, VkDevice device, PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr_, PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr_)
        {
            pfnVkGetInstanceProcAddr = pfnVkGetInstanceProcAddr_;
            pfnVkGetDeviceProcAddr = pfnVkGetDeviceProcAddr_;

            if (pfnVkGetInstanceProcAddr)
            {
                pfnVkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)pfnVkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties");
                pfnVkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)pfnVkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
            }

            if (pfnVkGetDeviceProcAddr)
            {
                pfnVkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)pfnVkGetDeviceProcAddr(device, "vkAllocateCommandBuffers");
                pfnVkAllocateMemory = (PFN_vkAllocateMemory)pfnVkGetDeviceProcAddr(device, "vkAllocateMemory");
                pfnVkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)pfnVkGetDeviceProcAddr(device, "vkBeginCommandBuffer");
                pfnVkBindBufferMemory = (PFN_vkBindBufferMemory)pfnVkGetDeviceProcAddr(device, "vkBindBufferMemory");
                pfnVkCreateBuffer = (PFN_vkCreateBuffer)pfnVkGetDeviceProcAddr(device, "vkCreateBuffer");
                pfnVkCreateCommandPool = (PFN_vkCreateCommandPool)pfnVkGetDeviceProcAddr(device, "vkCreateCommandPool");
                pfnVkCreateFence = (PFN_vkCreateFence)pfnVkGetDeviceProcAddr(device, "vkCreateFence");
                pfnVkDestroyBuffer = (PFN_vkDestroyBuffer)pfnVkGetDeviceProcAddr(device, "vkDestroyBuffer");
                pfnVkDestroyCommandPool = (PFN_vkDestroyCommandPool)pfnVkGetDeviceProcAddr(device, "vkDestroyCommandPool");
                pfnVkDestroyFence = (PFN_vkDestroyFence)pfnVkGetDeviceProcAddr(device, "vkDestroyFence");
                pfnVkEndCommandBuffer = (PFN_vkEndCommandBuffer)pfnVkGetDeviceProcAddr(device, "vkEndCommandBuffer");
                pfnVkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)pfnVkGetDeviceProcAddr(device, "vkFreeCommandBuffers");
                pfnVkFreeMemory = (PFN_vkFreeMemory)pfnVkGetDeviceProcAddr(device, "vkFreeMemory");
                pfnVkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)pfnVkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements");
                pfnVkGetFenceStatus = (PFN_vkGetFenceStatus)pfnVkGetDeviceProcAddr(device, "vkGetFenceStatus");
                pfnVkMapMemory = (PFN_vkMapMemory)pfnVkGetDeviceProcAddr(device, "vkMapMemory");
                pfnVkQueueSubmit = (PFN_vkQueueSubmit)pfnVkGetDeviceProcAddr(device, "vkQueueSubmit");
                pfnVkQueueWaitIdle = (PFN_vkQueueWaitIdle)pfnVkGetDeviceProcAddr(device, "vkQueueWaitIdle");
                pfnVkResetCommandBuffer = (PFN_vkResetCommandBuffer)pfnVkGetDeviceProcAddr(device, "vkResetCommandBuffer");
                pfnVkResetFences = (PFN_vkResetFences)pfnVkGetDeviceProcAddr(device, "vkResetFences");
                pfnVkUnmapMemory = (PFN_vkUnmapMemory)pfnVkGetDeviceProcAddr(device, "vkUnmapMemory");
                pfnVkWaitForFences = (PFN_vkWaitForFences)pfnVkGetDeviceProcAddr(device, "vkWaitForFences");
                pfnVkCreateQueryPool = (PFN_vkCreateQueryPool)pfnVkGetDeviceProcAddr(device, "vkCreateQueryPool");
                pfnVkDestroyQueryPool = (PFN_vkDestroyQueryPool)pfnVkGetDeviceProcAddr(device, "vkDestroyQueryPool");
                pfnVkCmdWriteTimestamp = (PFN_vkCmdWriteTimestamp)pfnVkGetDeviceProcAddr(device, "vkCmdWriteTimestamp");
                pfnVkCmdResetQueryPool = (PFN_vkCmdResetQueryPool)pfnVkGetDeviceProcAddr(device, "vkCmdResetQueryPool");
                pfnVkGetQueryPoolResults = (PFN_vkGetQueryPoolResults)pfnVkGetDeviceProcAddr(device, "vkGetQueryPoolResults");
            }
        }
#else
        void Initialize()
        {
            pfnVkGetInstanceProcAddr = vkGetInstanceProcAddr;
            pfnVkGetDeviceProcAddr = vkGetDeviceProcAddr;

            pfnVkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
            pfnVkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;

            pfnVkAllocateCommandBuffers = vkAllocateCommandBuffers;
            pfnVkAllocateMemory = vkAllocateMemory;
            pfnVkBeginCommandBuffer = vkBeginCommandBuffer;
            pfnVkBindBufferMemory = vkBindBufferMemory;
            pfnVkCreateBuffer = vkCreateBuffer;
            pfnVkCreateCommandPool = vkCreateCommandPool;
            pfnVkCreateFence = vkCreateFence;
            pfnVkDestroyBuffer = vkDestroyBuffer;
            pfnVkDestroyCommandPool = vkDestroyCommandPool;
            pfnVkDestroyFence = vkDestroyFence;
            pfnVkEndCommandBuffer = vkEndCommandBuffer;
            pfnVkFreeCommandBuffers = vkFreeCommandBuffers;
            pfnVkFreeMemory = vkFreeMemory;
            pfnVkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
            pfnVkGetFenceStatus = vkGetFenceStatus;
            pfnVkMapMemory = vkMapMemory;
            pfnVkQueueSubmit = vkQueueSubmit;
            pfnVkQueueWaitIdle = vkQueueWaitIdle;
            pfnVkResetCommandBuffer = vkResetCommandBuffer;
            pfnVkResetFences = vkResetFences;
            pfnVkUnmapMemory = vkUnmapMemory;
            pfnVkWaitForFences = vkWaitForFences;
            pfnVkCreateQueryPool = vkCreateQueryPool;
            pfnVkDestroyQueryPool = vkDestroyQueryPool;
            pfnVkCmdWriteTimestamp = vkCmdWriteTimestamp;
            pfnVkCmdResetQueryPool = vkCmdResetQueryPool;
            pfnVkGetQueryPoolResults = vkGetQueryPoolResults;
        }
#endif

        void Reset()
        {
            pfnVkGetInstanceProcAddr = nullptr;
            pfnVkGetDeviceProcAddr = nullptr;

            pfnVkGetPhysicalDeviceMemoryProperties = nullptr;
            pfnVkGetPhysicalDeviceProperties = nullptr;

            pfnVkAllocateCommandBuffers = nullptr;
            pfnVkAllocateMemory = nullptr;
            pfnVkBeginCommandBuffer = nullptr;
            pfnVkBindBufferMemory = nullptr;
            pfnVkCreateBuffer = nullptr;
            pfnVkCreateCommandPool = nullptr;
            pfnVkCreateFence = nullptr;
            pfnVkDestroyBuffer = nullptr;
            pfnVkDestroyCommandPool = nullptr;
            pfnVkDestroyFence = nullptr;
            pfnVkEndCommandBuffer = nullptr;
            pfnVkFreeCommandBuffers = nullptr;
            pfnVkFreeMemory = nullptr;
            pfnVkGetBufferMemoryRequirements = nullptr;
            pfnVkGetFenceStatus = nullptr;
            pfnVkMapMemory = nullptr;
            pfnVkQueueSubmit = nullptr;
            pfnVkQueueWaitIdle = nullptr;
            pfnVkResetCommandBuffer = nullptr;
            pfnVkResetFences = nullptr;
            pfnVkUnmapMemory = nullptr;
            pfnVkWaitForFences = nullptr;
            pfnVkCreateQueryPool = nullptr;
            pfnVkDestroyQueryPool = nullptr;
            pfnVkCmdWriteTimestamp = nullptr;
            pfnVkCmdResetQueryPool = nullptr;
            pfnVkGetQueryPoolResults = nullptr;
        }
    };

    inline std::string VulkanGetDeviceName(VkPhysicalDevice physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                          , VkInstance instance
                                          , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
#endif
        )
    {
        PFN_vkGetPhysicalDeviceProperties pfnVkGetPhysicalDeviceProperties =
#if defined(VK_NO_PROTOTYPES)
            (PFN_vkGetPhysicalDeviceProperties)pfnVkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
#else
            vkGetPhysicalDeviceProperties;
#endif
        VkPhysicalDeviceProperties properties;
        pfnVkGetPhysicalDeviceProperties(physicalDevice, &properties);
        return properties.deviceName;
    }

    inline bool VulkanIsNvidiaDevice(VkPhysicalDevice physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                    , VkInstance instance
                                    , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
#endif
        )
    {
        PFN_vkGetPhysicalDeviceProperties pfnVkGetPhysicalDeviceProperties =
#if defined(VK_NO_PROTOTYPES)
            (PFN_vkGetPhysicalDeviceProperties)pfnVkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
#else
            vkGetPhysicalDeviceProperties;
#endif
        VkPhysicalDeviceProperties properties;
        pfnVkGetPhysicalDeviceProperties(physicalDevice, &properties);
        if (properties.vendorID != NVIDIA_VENDOR_ID)
        {
            return false;
        }

        return true;
    }

    inline uint32_t VulkanGetInstanceApiVersion(
#if defined(VK_NO_PROTOTYPES)
        PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
#endif
        )
    {
#if !defined(VK_NO_PROTOTYPES)
        PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr = vkGetInstanceProcAddr;
#endif
        PFN_vkEnumerateInstanceVersion pfnVkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)pfnVkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
        //This API doesn't exist on 1.0 loader
        if (!pfnVkEnumerateInstanceVersion)
        {
            return VK_API_VERSION_1_0;
        }

        uint32_t loaderVersion;
        VkResult res = pfnVkEnumerateInstanceVersion(&loaderVersion);
        if (res != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Couldn't enumerate instance version!\n");
            return 0;
        }
        return loaderVersion;
    }

    inline uint32_t VulkanGetPhysicalDeviceApiVersion(VkPhysicalDevice physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                     , VkInstance instance
                                                     , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
#endif
        )
    {
        PFN_vkGetPhysicalDeviceProperties pfnVkGetPhysicalDeviceProperties =
#if defined(VK_NO_PROTOTYPES)
            (PFN_vkGetPhysicalDeviceProperties)pfnVkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
#else
            vkGetPhysicalDeviceProperties;
#endif
        VkPhysicalDeviceProperties properties;
        pfnVkGetPhysicalDeviceProperties(physicalDevice, &properties);
        return properties.apiVersion;
    }

    //
    // Vulkan NvPerf Utilities
    //
    inline bool VulkanAppendInstanceRequiredExtensions(std::vector<const char*>& instanceExtensionNames, uint32_t apiVersion)
    {
        NVPW_VK_Profiler_GetRequiredInstanceExtensions_Params getRequiredInstanceExtensionsParams = { NVPW_VK_Profiler_GetRequiredInstanceExtensions_Params_STRUCT_SIZE };
        getRequiredInstanceExtensionsParams.apiVersion = apiVersion;

        NVPA_Status nvpaStatus = NVPW_VK_Profiler_GetRequiredInstanceExtensions(&getRequiredInstanceExtensionsParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_GetRequiredInstanceExtensions failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }

        if (!getRequiredInstanceExtensionsParams.isOfficiallySupportedVersion)
        {
            uint32_t major = VK_VERSION_MAJOR(getRequiredInstanceExtensionsParams.apiVersion);
            uint32_t minor = VK_VERSION_MINOR(getRequiredInstanceExtensionsParams.apiVersion);
            uint32_t patch = VK_VERSION_PATCH(getRequiredInstanceExtensionsParams.apiVersion);
            // not an error - NvPerf treats any unknown version as the same as its latest known version.
            //                Unknown version warnings should be reported back to the Nsight Perf team to get official support
            NV_PERF_LOG_WRN(10, "Vulkan Instance API Version: %u.%u.%u - is not an officially supported version\n", major, minor, patch);
        }

        for (uint32_t extensionIndex=0; extensionIndex < getRequiredInstanceExtensionsParams.numInstanceExtensionNames; ++ extensionIndex)
        {
            instanceExtensionNames.push_back(getRequiredInstanceExtensionsParams.ppInstanceExtensionNames[extensionIndex]);
        }
        return true;
    }

    inline bool VulkanAppendDeviceRequiredExtensions(VkInstance instance, VkPhysicalDevice physicalDevice, void* pfnVkGetInstanceProcAddr, std::vector<const char*>& deviceExtensionNames)
    {
        if (!VulkanIsNvidiaDevice(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                 , instance
                                 , (PFN_vkGetInstanceProcAddr)pfnVkGetInstanceProcAddr
#endif
            ))
        {
            return true; // do nothing on non-NVIDIA devices
        }

        NVPW_VK_Profiler_GetRequiredDeviceExtensions_Params getRequiredDeviceExtensionsParams = { NVPW_VK_Profiler_GetRequiredDeviceExtensions_Params_STRUCT_SIZE };
        getRequiredDeviceExtensionsParams.apiVersion = VulkanGetPhysicalDeviceApiVersion(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                                                        , instance
                                                                                        , reinterpret_cast<PFN_vkGetInstanceProcAddr>(pfnVkGetInstanceProcAddr)
#endif
                                                                                        );

        // optional parameters - this allows NvPerf to query if certain advanced features are available for use
        getRequiredDeviceExtensionsParams.instance = instance;
        getRequiredDeviceExtensionsParams.physicalDevice = physicalDevice;
        getRequiredDeviceExtensionsParams.pfnGetInstanceProcAddr = pfnVkGetInstanceProcAddr;

        NVPA_Status nvpaStatus = NVPW_VK_Profiler_GetRequiredDeviceExtensions(&getRequiredDeviceExtensionsParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_GetRequiredDeviceExtensions failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }

        if (!getRequiredDeviceExtensionsParams.isOfficiallySupportedVersion)
        {
            uint32_t major = VK_VERSION_MAJOR(getRequiredDeviceExtensionsParams.apiVersion);
            uint32_t minor = VK_VERSION_MINOR(getRequiredDeviceExtensionsParams.apiVersion);
            uint32_t patch = VK_VERSION_PATCH(getRequiredDeviceExtensionsParams.apiVersion);
            // not an error - NvPerf treats any unknown version as the same as its latest known version.
            //                Unknown version warnings should be reported back to the Nsight Perf team to get official support
            NV_PERF_LOG_WRN(100, "Vulkan Device API Version: %u.%u.%u - is not an officially supported version\n", major, minor, patch);
        }

        for (uint32_t extensionIndex=0; extensionIndex < getRequiredDeviceExtensionsParams.numDeviceExtensionNames; ++ extensionIndex)
        {
            deviceExtensionNames.push_back(getRequiredDeviceExtensionsParams.ppDeviceExtensionNames[extensionIndex]);
        }

        return true;
    }

    inline bool VulkanAppendRequiredExtensions(std::vector<const char*>& instanceExtensionNames, std::vector<const char*>& deviceExtensionNames, uint32_t apiVersion, VkInstance instance = VK_NULL_HANDLE, VkPhysicalDevice physicalDevice = VK_NULL_HANDLE, PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr = nullptr)
    {
        bool status = VulkanAppendInstanceRequiredExtensions(instanceExtensionNames, apiVersion);
        if (!status)
        {
            return false;
        }

        status = VulkanAppendDeviceRequiredExtensions(instance, physicalDevice, (void*)pfnVkGetInstanceProcAddr, deviceExtensionNames);
        if (!status)
        {
            return false;
        }

        return true;
    }

    inline bool VulkanLoadDriver(VkInstance instance)
    {
        NVPW_VK_LoadDriver_Params loadDriverParams = { NVPW_VK_LoadDriver_Params_STRUCT_SIZE };
        loadDriverParams.instance = instance;
        NVPA_Status nvpaStatus = NVPW_VK_LoadDriver(&loadDriverParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_LoadDriver failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }
        return true;
    }

    inline size_t VulkanGetNvperfDeviceIndex(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                                            , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                                            , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
                                            , size_t sliIndex = 0)
    {
        NVPW_VK_Device_GetDeviceIndex_Params getDeviceIndexParams = { NVPW_VK_Device_GetDeviceIndex_Params_STRUCT_SIZE };
        getDeviceIndexParams.instance = instance;
        getDeviceIndexParams.physicalDevice = physicalDevice;
        getDeviceIndexParams.device = device;
        getDeviceIndexParams.sliIndex = sliIndex;
#if defined(VK_NO_PROTOTYPES)
        getDeviceIndexParams.pfnGetInstanceProcAddr = (void*)pfnVkGetInstanceProcAddr;
        getDeviceIndexParams.pfnGetDeviceProcAddr = (void*)pfnVkGetDeviceProcAddr;
#else
        getDeviceIndexParams.pfnGetInstanceProcAddr = (void*)vkGetInstanceProcAddr;
        getDeviceIndexParams.pfnGetDeviceProcAddr = (void*)vkGetDeviceProcAddr;
#endif

        NVPA_Status nvpaStatus = NVPW_VK_Device_GetDeviceIndex(&getDeviceIndexParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_VK_Device_GetDeviceIndex failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return ~size_t(0);
        }

        return getDeviceIndexParams.deviceIndex;
    }

    inline DeviceIdentifiers VulkanGetDeviceIdentifiers(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                                                       , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                                                       , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
                                                       , size_t sliIndex = 0)
    {
        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                             , pfnVkGetInstanceProcAddr
                                                             , pfnVkGetDeviceProcAddr
#endif
                                                             , sliIndex);

        DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(deviceIndex);
        return deviceIdentifiers;
    }

    inline ClockInfo VulkanGetDeviceClockState(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                                                            , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                                                            , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
        )
    {
        size_t nvperfDeviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                             , pfnVkGetInstanceProcAddr
                                                             , pfnVkGetDeviceProcAddr
#endif
            );
        return GetDeviceClockState(nvperfDeviceIndex);
    }

    inline bool VulkanSetDeviceClockState(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, NVPW_Device_ClockSetting clockSetting
#if defined(VK_NO_PROTOTYPES)
                                         , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                                         , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
        )
    {
        size_t nvperfDeviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                             , pfnVkGetInstanceProcAddr
                                                             , pfnVkGetDeviceProcAddr
#endif
            );
        return SetDeviceClockState(nvperfDeviceIndex, clockSetting);
    }

    inline bool VulkanSetDeviceClockState(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, const ClockInfo& clockInfo
#if defined(VK_NO_PROTOTYPES)
                                         , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                                         , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
        )
    {
        size_t nvperfDeviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                             , pfnVkGetInstanceProcAddr
                                                             , pfnVkGetDeviceProcAddr
#endif
                );
        return SetDeviceClockState(nvperfDeviceIndex, clockInfo);
    }

    inline size_t VulkanCalculateMetricsEvaluatorScratchBufferSize(const char* pChipName)
    {
        NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Params calculateScratchBufferSizeParams = { NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE };
        calculateScratchBufferSizeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize(&calculateScratchBufferSizeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return 0;
        }
        return calculateScratchBufferSizeParams.scratchBufferSize;
    }

    inline NVPW_MetricsEvaluator* VulkanCreateMetricsEvaluator(uint8_t* pScratchBuffer, size_t scratchBufferSize, const char* pChipName)
    {
        NVPW_VK_MetricsEvaluator_Initialize_Params initializeParams = { NVPW_VK_MetricsEvaluator_Initialize_Params_STRUCT_SIZE };
        initializeParams.pScratchBuffer = pScratchBuffer;
        initializeParams.scratchBufferSize = scratchBufferSize;
        initializeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_VK_MetricsEvaluator_Initialize(&initializeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_VK_MetricsEvaluator_Initialize failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return nullptr;
        }
        return initializeParams.pMetricsEvaluator;
    }

}}

namespace nv { namespace perf { namespace profiler {

    inline NVPW_RawCounterConfig* VulkanCreateRawCounterConfig(const char* pChipName)
    {
        NVPW_VK_RawCounterConfig_Create_Params configParams = { NVPW_VK_RawCounterConfig_Create_Params_STRUCT_SIZE };
        configParams.activityKind = NVPA_ACTIVITY_KIND_PROFILER;
        configParams.pChipName = pChipName;

        NVPA_Status nvpaStatus = NVPW_VK_RawCounterConfig_Create(&configParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_VK_RawCounterConfig_Create failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return nullptr;
        }

        return configParams.pRawCounterConfig;
    }

    inline bool VulkanIsGpuSupported(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                                    , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                                    , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
                                    , size_t sliIndex = 0)
    {
        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                             , pfnVkGetInstanceProcAddr
                                                             , pfnVkGetDeviceProcAddr
#endif
                                                             , sliIndex);

        NVPW_VK_Profiler_IsGpuSupported_Params params = { NVPW_VK_Profiler_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_VK_Profiler_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            std::string deviceName = VulkanGetDeviceName(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                        , instance
                                                        , pfnVkGetInstanceProcAddr
#endif
                                                        );
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_IsGpuSupported failed on %s, nvpaStatus = %s\n", deviceName.c_str(), FormatStatus(nvpaStatus).c_str());
            return false;
        }

        if (!params.isSupported)
        {
            std::string deviceName = VulkanGetDeviceName(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                        , instance
                                                        , pfnVkGetInstanceProcAddr
#endif
                                                        );
            NV_PERF_LOG_ERR(10, "%s is not supported for profiling\n", deviceName.c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = VulkanGetDeviceIdentifiers(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                                                      , pfnVkGetInstanceProcAddr
                                                                                      , pfnVkGetDeviceProcAddr
#endif
                                                                                      , sliIndex);
                NV_PERF_LOG_ERR(10, "Unsupported GPU architecture %s\n", deviceIdentifiers.pChipName);
            }
            if (params.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Devices in SLI configuration are not supported.\n");
            }
            if (params.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Cryptomining GPUs (NVIDIA CMP) are not supported.\n");
            }
            return false;
        }

        return true;
    }

    inline bool VulkanPushRange(VkCommandBuffer commandBuffer, const char* pRangeName)
    {
        NVPW_VK_Profiler_CommandBuffer_PushRange_Params pushRangeParams = { NVPW_VK_Profiler_CommandBuffer_PushRange_Params_STRUCT_SIZE };
        pushRangeParams.pRangeName = pRangeName;
        pushRangeParams.rangeNameLength = 0;
        pushRangeParams.commandBuffer = commandBuffer;
        NVPA_Status nvpaStatus = NVPW_VK_Profiler_CommandBuffer_PushRange(&pushRangeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_VK_Profiler_CommandBuffer_PushRange failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }
        return true;
    }
    inline bool VulkanPopRange(VkCommandBuffer commandBuffer)
    {
        NVPW_VK_Profiler_CommandBuffer_PopRange_Params popParams = { NVPW_VK_Profiler_CommandBuffer_PopRange_Params_STRUCT_SIZE };
        popParams.commandBuffer = commandBuffer;
        NVPA_Status nvpaStatus = NVPW_VK_Profiler_CommandBuffer_PopRange(&popParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_VK_Profiler_CommandBuffer_PopRange failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }
        return true;
    }

    inline bool VulkanPushRange_Nop(VkCommandBuffer commandBuffer, const char* pRangeName)
    {
        return false;
    }
    inline bool VulkanPopRange_Nop(VkCommandBuffer commandBuffer)
    {
        return false;
    }

    // 
    struct VulkanRangeCommands
    {
        bool isNvidiaDevice;
        bool(*PushRange)(VkCommandBuffer commandBuffer, const char* pRangeName);
        bool(*PopRange)(VkCommandBuffer commandBuffer);

    public:
        VulkanRangeCommands()
            : isNvidiaDevice(false)
            , PushRange(&VulkanPushRange_Nop)
            , PopRange(&VulkanPopRange_Nop)
        {
        }

        void Initialize(bool isNvidiaDevice_)
        {
            isNvidiaDevice = isNvidiaDevice_;
            if (isNvidiaDevice_)
            {
                PushRange = &VulkanPushRange;
                PopRange = &VulkanPopRange;
            }
            else
            {
                PushRange = &VulkanPushRange_Nop;
                PopRange = &VulkanPopRange_Nop;
            }
        }

        void Initialize(VkPhysicalDevice physicalDevice
#if defined(VK_NO_PROTOTYPES)
                       , VkInstance instance
                       , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
#endif
            )
        {
            const bool isNvidiaDevice_ = VulkanIsNvidiaDevice(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                             , instance
                                                             , pfnVkGetInstanceProcAddr
#endif
                                                             );
            return Initialize(isNvidiaDevice_);
        }
    };

}}} // nv::perf::profiler

namespace nv { namespace perf { namespace mini_trace {

    inline bool VulkanIsGpuSupported(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                                    , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                                    , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
                                    , size_t sliIndex = 0)
    {
        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                             , pfnVkGetInstanceProcAddr
                                                             , pfnVkGetDeviceProcAddr
#endif
                                                             , sliIndex);
        if (deviceIndex == ~size_t(0))
        {
            std::string deviceName = VulkanGetDeviceName(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                        , instance
                                                        , pfnVkGetInstanceProcAddr
#endif
                                                        );
            NV_PERF_LOG_ERR(10, "VulkanGetNvperfDeviceIndex failed on %ls\n", deviceName.c_str());

            return false;
        }

        NVPW_VK_MiniTrace_IsGpuSupported_Params params = { NVPW_VK_MiniTrace_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            std::string deviceName = VulkanGetDeviceName(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                        , instance
                                                        , pfnVkGetInstanceProcAddr
#endif
                                                        );
            NV_PERF_LOG_ERR(10, "NVPW_VK_MiniTrace_IsGpuSupported failed on %s, nvpaStatus = %s\n", deviceName.c_str(), FormatStatus(nvpaStatus).c_str());
            return false;
        }

        if (!params.isSupported)
        {
            std::string deviceName = VulkanGetDeviceName(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                        , instance
                                                        , pfnVkGetInstanceProcAddr
#endif
                                                        );
            NV_PERF_LOG_ERR(10, "%s is not supported for profiling\n", deviceName.c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = VulkanGetDeviceIdentifiers(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                                                      , pfnVkGetInstanceProcAddr
                                                                                      , pfnVkGetDeviceProcAddr
#endif
                                                                                      , sliIndex);
                NV_PERF_LOG_ERR(10, "Unsupported GPU architecture %s\n", deviceIdentifiers.pChipName);
            }
            if (params.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Devices in SLI configuration are not supported.\n");
            }
            if (params.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Cryptomining GPUs (NVIDIA CMP) are not supported.\n");
            }
            return false;
        }

        return true;
    }

}}} // nv::perf::mini_trace

namespace nv { namespace perf { namespace sampler {

    inline bool VulkanIsGpuSupported(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                                    , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                                    , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
                                    , size_t sliIndex = 0)
    {
        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                             , pfnVkGetInstanceProcAddr
                                                             , pfnVkGetDeviceProcAddr
#endif
                                                             , sliIndex);
        if (deviceIndex == ~size_t(0))
        {
            std::string deviceName = VulkanGetDeviceName(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                        , instance
                                                        , pfnVkGetInstanceProcAddr
#endif
                                                        );
            NV_PERF_LOG_ERR(10, "VulkanGetNvperfDeviceIndex failed on %ls\n", deviceName.c_str());
            return false;
        }
        if (!GpuPeriodicSamplerIsGpuSupported(deviceIndex))
        {
            return false;
        }
        if (!mini_trace::VulkanIsGpuSupported(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                             , pfnVkGetInstanceProcAddr
                                             , pfnVkGetDeviceProcAddr
#endif
                                             , sliIndex))
        {
            return false;
        }
        return true;
    }

}}} // nv::perf::sampler
