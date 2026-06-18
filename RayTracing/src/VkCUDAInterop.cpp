#include "VkCUDAInterop.h"
#include "Peanut/Application.h"
#include <stdexcept>
#include <cstring>

static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(Peanut::Application::GetPhysicalDevice(), &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    throw std::runtime_error("VkCUDAInterop: no suitable memory type");
}

VkCUDAInterop::VkCUDAInterop(uint32_t width, uint32_t height)
    : m_Width(width), m_Height(height), m_Size(width * height * sizeof(uint32_t))
{
    CreateVulkanBuffer();
    try {
        ExportToCUDA();
    } catch (...) {
        // Clean up Vulkan resources if CUDA import fails
        if (m_Memory) vkFreeMemory(Peanut::Application::GetDevice(), m_Memory, nullptr);
        if (m_Buffer) vkDestroyBuffer(Peanut::Application::GetDevice(), m_Buffer, nullptr);
        m_Memory = VK_NULL_HANDLE;
        m_Buffer = VK_NULL_HANDLE;
        throw;
    }
}

VkCUDAInterop::~VkCUDAInterop() noexcept
{
    VkDevice device = Peanut::Application::GetDevice();
    if (m_CUDAExtMem) cudaDestroyExternalMemory(m_CUDAExtMem);
    if (m_Memory) vkFreeMemory(device, m_Memory, nullptr);
    if (m_Buffer) vkDestroyBuffer(device, m_Buffer, nullptr);
}

void VkCUDAInterop::CreateVulkanBuffer()
{
    VkDevice device = Peanut::Application::GetDevice();

    VkExternalMemoryBufferCreateInfo extInfo = {};
    extInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    extInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = &extInfo;
    bufferInfo.size = m_Size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_Buffer) != VK_SUCCESS)
        throw std::runtime_error("VkCUDAInterop: vkCreateBuffer failed");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, m_Buffer, &memReq);

    VkMemoryDedicatedAllocateInfo dedicatedInfo = {};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.buffer = m_Buffer;

    VkExportMemoryAllocateInfo exportInfo = {};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    exportInfo.pNext = nullptr;
    dedicatedInfo.pNext = &exportInfo;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS)
        throw std::runtime_error("VkCUDAInterop: vkAllocateMemory failed");

    if (vkBindBufferMemory(device, m_Buffer, m_Memory, 0) != VK_SUCCESS)
        throw std::runtime_error("VkCUDAInterop: vkBindBufferMemory failed");
}

void VkCUDAInterop::ExportToCUDA()
{
    VkDevice device = Peanut::Application::GetDevice();

    auto vkGetMemoryWin32HandleKHR = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR"));
    if (!vkGetMemoryWin32HandleKHR)
        throw std::runtime_error("VkCUDAInterop: vkGetMemoryWin32HandleKHR not available");

    VkMemoryGetWin32HandleInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.memory = m_Memory;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    HANDLE handle = nullptr;
    if (vkGetMemoryWin32HandleKHR(device, &handleInfo, &handle) != VK_SUCCESS)
        throw std::runtime_error("VkCUDAInterop: vkGetMemoryWin32HandleKHR failed");

    cudaExternalMemoryHandleDesc extMemDesc = {};
    extMemDesc.type = cudaExternalMemoryHandleTypeOpaqueWin32;
    extMemDesc.handle.win32.handle = handle;
    extMemDesc.size = m_Size;
    extMemDesc.flags = cudaExternalMemoryDedicated;

    cudaError_t err = cudaImportExternalMemory(&m_CUDAExtMem, &extMemDesc);
    CloseHandle(handle);
    if (err != cudaSuccess)
        throw std::runtime_error(std::string("VkCUDAInterop: cudaImportExternalMemory failed: ") + cudaGetErrorString(err));

    cudaExternalMemoryBufferDesc bufDesc = {};
    bufDesc.size = m_Size;

    err = cudaExternalMemoryGetMappedBuffer(&m_CUDADevPtr, m_CUDAExtMem, &bufDesc);
    if (err != cudaSuccess)
        throw std::runtime_error(std::string("VkCUDAInterop: cudaExternalMemoryGetMappedBuffer failed: ") + cudaGetErrorString(err));
}

void VkCUDAInterop::SyncCUDAComplete(cudaStream_t stream) noexcept
{
    cudaError_t err = cudaStreamSynchronize(stream);
    if (err != cudaSuccess)
        std::fprintf(stderr, "[Interop] cudaStreamSynchronize failed: %s\n", cudaGetErrorString(err));
}
