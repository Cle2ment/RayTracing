#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <cuda_runtime.h>
#include <cstdint>

class VkCUDAInterop
{
public:
    VkCUDAInterop(uint32_t width, uint32_t height);
    ~VkCUDAInterop() noexcept;

    void*  GetCUDADevicePtr() const noexcept { return m_CUDADevPtr; }
    VkBuffer GetVulkanBuffer() const noexcept { return m_Buffer; }
    uint32_t GetWidth()  const noexcept { return m_Width; }
    uint32_t GetHeight() const noexcept { return m_Height; }

    void SyncCUDAComplete(cudaStream_t stream) noexcept;

private:
    void CreateVulkanBuffer();
    void ExportToCUDA();

    uint32_t m_Width, m_Height;
    VkDeviceSize m_Size;

    VkBuffer       m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;

    cudaExternalMemory_t m_CUDAExtMem = nullptr;
    void*                m_CUDADevPtr = nullptr;
};
