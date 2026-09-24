#include <cassert>
#include <glm/glm.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <print>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>

#define VkCheck(x)                                                    \
    {                                                                 \
        VkResult result = x;                                          \
        if (result != VK_SUCCESS)                                     \
        {                                                             \
            std::println("Error: {} code: {}", #x, (uint32_t)result); \
        }                                                             \
    }

namespace vk
{

VkAllocationCallbacks *GetAllocationCallback()
{
    // TODO: Return actual allocation callbacks
    return nullptr;
}

class Window
{
public:
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window() : mHandle(nullptr) {}
    Window(Window &&window) noexcept
    {
        mHandle = window.mHandle;
        window.mHandle = nullptr;
    }
    Window &operator=(Window &&window) noexcept
    {
        glfwDestroyWindow(mHandle);
        mHandle = window.mHandle;
        window.mHandle = nullptr;
        return *this;
    }
    Window(uint32_t width, uint32_t height, std::string_view title)
    {
        if (mWindowCount == 0)
        {
            glfwInit();
        }

        mWindowCount++;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        mHandle = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);

        // TODO: Support for fullscreen window creation
    }
    ~Window()
    {
        mWindowCount--;
        glfwDestroyWindow(mHandle);
    }

    GLFWwindow *GetHandle() const
    {
        return mHandle;
    }

    bool ShouldClose()
    {
        return glfwWindowShouldClose(mHandle);
    }

    std::vector<const char *> GetInstanceExtensions()
    {
        uint32_t count = 0;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&count);

        std::vector<const char *> extensions;

        for (uint32_t i = 0; i < count; i++)
        {
            extensions.push_back(glfwExtensions[i]);
        }

        return extensions;
    }

private:
    GLFWwindow *mHandle;
    static uint32_t mWindowCount;
};

uint32_t Window::mWindowCount = 0;

class Instance
{
public:
    Instance(const Instance &) = delete;
    Instance &operator=(const Instance &) = delete;
    Instance(Instance &&instance) noexcept
    {
        mHandle = instance.mHandle;
        instance.mHandle = VK_NULL_HANDLE;
    }
    Instance &operator=(Instance &&instance) noexcept
    {
        vkDestroyInstance(mHandle, GetAllocationCallback());
        mHandle = instance.mHandle;
        instance.mHandle = VK_NULL_HANDLE;
        return *this;
    }
    Instance() : mHandle(VK_NULL_HANDLE) {}
    Instance(const std::vector<const char *> &extensions, const std::vector<const char *> &layers, std::string_view applicationName = "", std::string_view engineName = "")
    {
        VkApplicationInfo appInfo =
            {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = applicationName.data(),
                .pEngineName = engineName.data(),
                .apiVersion = VK_API_VERSION_1_4,

            };
        VkInstanceCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo = &appInfo,
                .enabledLayerCount = (uint32_t)layers.size(),
                .ppEnabledLayerNames = layers.data(),
                .enabledExtensionCount = (uint32_t)extensions.size(),
                .ppEnabledExtensionNames = extensions.data(),
            };

        VkCheck(vkCreateInstance(&createInfo, GetAllocationCallback(), &mHandle));
    }
    ~Instance()
    {
        vkDestroyInstance(mHandle, GetAllocationCallback());
    }

    VkInstance GetHandle() const
    {
        return mHandle;
    }

private:
    VkInstance mHandle;
};

class PhysicalDevice
{
public:
    PhysicalDevice() : mHandle(VK_NULL_HANDLE) {}
    PhysicalDevice(const PhysicalDevice &) = delete;
    PhysicalDevice &operator=(const PhysicalDevice &) = delete;
    PhysicalDevice(PhysicalDevice &&physicalDevice) noexcept
    {
        mHandle = physicalDevice.mHandle;
        physicalDevice.mHandle = VK_NULL_HANDLE;
    }
    PhysicalDevice &operator=(PhysicalDevice &&physicalDevice) noexcept
    {
        mHandle = physicalDevice.mHandle;
        physicalDevice.mHandle = VK_NULL_HANDLE;
        return *this;
    }
    PhysicalDevice(const Instance &instance, VkPhysicalDeviceType deviceType)
    {
        assert(instance.GetHandle() != VK_NULL_HANDLE);
        uint32_t count = 0;
        VkCheck(vkEnumeratePhysicalDevices(instance.GetHandle(), &count, nullptr));
        std::vector<VkPhysicalDevice> devices(count);
        VkCheck(vkEnumeratePhysicalDevices(instance.GetHandle(), &count, devices.data()));

        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties property;
            vkGetPhysicalDeviceProperties(device, &property);
            if (property.deviceType == deviceType)
            {
                mHandle = device;
                break;
            }
        }
    }
    ~PhysicalDevice()
    {
    }

    VkPhysicalDevice GetHandle() const
    {
        return mHandle;
    }

    const VkPhysicalDeviceProperties &GetProperties() const
    {
        return mProperties;
    }

private:
    VkPhysicalDevice mHandle;
    VkPhysicalDeviceProperties mProperties;
};

class Device
{
public:
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&device) noexcept
    {
        mHandle = device.mHandle;
        device.mHandle = VK_NULL_HANDLE;
    }
    Device &operator=(Device &&device) noexcept
    {
        vkDestroyDevice(mHandle, GetAllocationCallback());
        mHandle = device.mHandle;
        device.mHandle = VK_NULL_HANDLE;
        return *this;
    }
    Device(const PhysicalDevice &physicalDevice, const std::vector<const char *> &extensions, const std::vector<const char *> &layers, VkSurfaceKHR surface = VK_NULL_HANDLE)
        : mGraphicsIndex(UINT32_MAX), mComputeIndex(UINT32_MAX), mPresentIndex(UINT32_MAX), mTransferIndex(UINT32_MAX),
          mGraphicsQueue(VK_NULL_HANDLE), mComputeQueue(VK_NULL_HANDLE), mTransferQueue(VK_NULL_HANDLE), mPresentQueue(VK_NULL_HANDLE),
          mHandle(VK_NULL_HANDLE)
    {

        PopularQueueIndices(physicalDevice, surface);

        float graphicsPriority = 1.f, computePriority = 1.f, transferPriority = 1.f, presentPriority = 1.f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfo = GetQueueCreateInfos(graphicsPriority, computePriority, transferPriority, presentPriority);

        VkDeviceCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .queueCreateInfoCount = (uint32_t)queueCreateInfo.size(),
                .pQueueCreateInfos = queueCreateInfo.data(),
                .enabledLayerCount = (uint32_t)layers.size(),
                .ppEnabledLayerNames = layers.data(),
                .enabledExtensionCount = (uint32_t)extensions.size(),
                .ppEnabledExtensionNames = extensions.data(),
            };

        VkCheck(vkCreateDevice(physicalDevice.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));

        PopulateQueues();
    }

    void Wait()
    {
        vkDeviceWaitIdle(mHandle);
    }

    ~Device()
    {
        vkDestroyDevice(mHandle, GetAllocationCallback());
    }

    VkQueue GetGraphicsQueue() const
    {
        return mGraphicsQueue;
    }
    VkQueue GetComputeQueue() const
    {
        return mComputeQueue;
    }
    VkQueue GetPresentQueue() const
    {
        return mPresentQueue;
    }
    VkQueue GetTransferQueue() const
    {
        return mTransferQueue;
    }

    VkDevice GetHandle() const
    {
        return mHandle;
    }

private:
    void PopularQueueIndices(const PhysicalDevice &physicalDevice, VkSurfaceKHR surface = VK_NULL_HANDLE)
    {

        // TODO: get queue indices of specific queue type with lower number of other queues,
        // eg: if one queue contains both compute and graphic queue, another queue contain just compute queue,
        // queue with only compute queue should be selected

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.GetHandle(), &count, nullptr);
        std::vector<VkQueueFamilyProperties> properties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.GetHandle(), &count, properties.data());

        for (uint32_t i = 0; i < properties.size(); i++)
        {
            if ((properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT && mGraphicsIndex == UINT32_MAX)
            {
                mGraphicsIndex = i;
            }
            if ((properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT && mComputeIndex == UINT32_MAX)
            {
                mComputeIndex = i;
            }
            if ((properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT && mTransferIndex == UINT32_MAX)
            {
                mTransferIndex = i;
            }
            if (surface != VK_NULL_HANDLE && mPresentIndex == UINT32_MAX)
            {
                VkBool32 supported = VK_FALSE;
                VkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice.GetHandle(), i, surface, &supported));
            }
        }
    }

    void PopulateQueues()
    {
        if (mGraphicsIndex != UINT32_MAX)
        {
            vkGetDeviceQueue(mHandle, mGraphicsIndex, 0, &mGraphicsQueue);
        }
        if (mComputeIndex != UINT32_MAX)
        {
            vkGetDeviceQueue(mHandle, mComputeIndex, 0, &mComputeQueue);
        }
        if (mTransferIndex != UINT32_MAX)
        {
            vkGetDeviceQueue(mHandle, mTransferIndex, 0, &mTransferQueue);
        }
        if (mPresentIndex != UINT32_MAX)
        {
            vkGetDeviceQueue(mHandle, mPresentIndex, 0, &mPresentQueue);
        }
    }

    std::vector<VkDeviceQueueCreateInfo> GetQueueCreateInfos(float &graphicPriority, float &computePriority, float &transferPriority, float &presentPriority)
    {
        std::vector<VkDeviceQueueCreateInfo> createInfos;
        if (mGraphicsIndex != UINT32_MAX)
        {
            VkDeviceQueueCreateInfo graphicQueueCreateInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = mGraphicsIndex,
                    .queueCount = 1,
                    .pQueuePriorities = &graphicPriority,
                };

            createInfos.push_back(graphicQueueCreateInfo);
        }
        if (mComputeIndex != UINT32_MAX && mComputeIndex != mGraphicsIndex)
        {
            VkDeviceQueueCreateInfo computeQueueCreateInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = mComputeIndex,
                    .queueCount = 1,
                    .pQueuePriorities = &computePriority,
                };
            createInfos.push_back(computeQueueCreateInfo);
        }
        if (mTransferIndex != UINT32_MAX && mTransferIndex != mGraphicsIndex && mTransferIndex != mComputeIndex)
        {
            VkDeviceQueueCreateInfo transferQueueCreateInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = mTransferIndex,
                    .queueCount = 1,
                    .pQueuePriorities = &transferPriority,
                };
            createInfos.push_back(transferQueueCreateInfo);
        }
        if (mPresentIndex != UINT32_MAX && mPresentIndex != mGraphicsIndex && mPresentIndex != mComputeIndex && mPresentIndex != mTransferIndex)
        {
            VkDeviceQueueCreateInfo presentQueueCreateInfo =
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = mPresentIndex,
                    .queueCount = 1,
                    .pQueuePriorities = &presentPriority,
                };
            createInfos.push_back(presentQueueCreateInfo);
        }

        return createInfos;
    }

    VkDevice mHandle;

    VkQueue mGraphicsQueue;
    VkQueue mComputeQueue;
    VkQueue mPresentQueue;
    VkQueue mTransferQueue;

    uint32_t mGraphicsIndex;
    uint32_t mComputeIndex;
    uint32_t mPresentIndex;
    uint32_t mTransferIndex;
};

class Semaphore
{
public:
    Semaphore() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}
    Semaphore(const Device &device)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        mDevice = device.GetHandle();

        VkSemaphoreCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
        VkCheck(vkCreateSemaphore(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }
    ~Semaphore()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkDestroySemaphore(mDevice, mHandle, GetAllocationCallback());
    }

    VkSemaphore GetHandle() const
    {
        return mHandle;
    }

private:
    VkSemaphore mHandle;
    VkDevice mDevice;
};

class Fence
{
public:
    Fence() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}
    Fence(const Device &device)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        mDevice = device.GetHandle();

        VkFenceCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            };
        VkCheck(vkCreateFence(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }
    ~Fence()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkDestroyFence(mDevice, mHandle, GetAllocationCallback());
    }

    void Wait(uint64_t timeout = UINT64_MAX)
    {
        VkCheck(vkWaitForFences(mDevice, 1, &mHandle, VK_TRUE, timeout));
    }

    void Reset()
    {
        vkResetFences(mDevice, 1, &mHandle);
    }

    VkFence GetHandle() const
    {
        return mHandle;
    }

private:
    VkFence mHandle;
    VkDevice mDevice;
};

class Surface
{
public:
    Surface() : mInstance(VK_NULL_HANDLE), mHandle(VK_NULL_HANDLE) {}
    Surface(const Surface &) = delete;
    Surface &operator=(const Surface &) = delete;
    Surface(Surface &&surface) noexcept
    {
        mHandle = surface.mHandle;
        mInstance = surface.mInstance;
        surface.mHandle = VK_NULL_HANDLE;
        surface.mInstance = VK_NULL_HANDLE;
    }
    Surface &operator=(Surface &&surface) noexcept
    {
        vkDestroySurfaceKHR(mInstance, mHandle, GetAllocationCallback());
        mHandle = surface.mHandle;
        mInstance = surface.mInstance;
        surface.mHandle = VK_NULL_HANDLE;
        surface.mInstance = VK_NULL_HANDLE;
        return *this;
    }
    Surface(const Instance &instance, GLFWwindow *window)
    {
        assert(instance.GetHandle() != VK_NULL_HANDLE);
        mInstance = instance.GetHandle();
        VkCheck(glfwCreateWindowSurface(instance.GetHandle(), window, GetAllocationCallback(), &mHandle));
    }

    ~Surface()
    {
        if (mInstance == VK_NULL_HANDLE)
            return;
        vkDestroySurfaceKHR(mInstance, mHandle, GetAllocationCallback());
    }

    VkSurfaceKHR GetHandle() const
    {
        return mHandle;
    }

private:
    VkSurfaceKHR mHandle;
    VkInstance mInstance;
};

class CommandPool
{
public:
    CommandPool() : mHandle(VK_NULL_HANDLE) {}
    CommandPool(const CommandPool &) = default;
    CommandPool &operator=(const CommandPool &) = default;
    CommandPool(CommandPool &&commandPool) noexcept
    {
        mHandle = commandPool.mHandle;
        mDevice = commandPool.mDevice;
        commandPool.mHandle = VK_NULL_HANDLE;
        commandPool.mDevice = VK_NULL_HANDLE;
    }
    CommandPool &operator=(CommandPool &&commandPool) noexcept
    {
        vkDestroyCommandPool(mDevice, mHandle, GetAllocationCallback());
        mHandle = commandPool.mHandle;
        mDevice = commandPool.mDevice;
        commandPool.mHandle = VK_NULL_HANDLE;
        commandPool.mDevice = VK_NULL_HANDLE;
        return *this;
    }
    CommandPool(const Device &device)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        VkCommandPoolCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            };

        mDevice = device.GetHandle();
        VkCheck(vkCreateCommandPool(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }

    ~CommandPool()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkDestroyCommandPool(mDevice, mHandle, GetAllocationCallback());
    }

    VkCommandPool GetHandle() const
    {
        return mHandle;
    }

private:
    VkCommandPool mHandle;
    VkDevice mDevice;
};

class CommandBuffer
{
public:
    CommandBuffer() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE), mCommandPool(VK_NULL_HANDLE) {}
    CommandBuffer(const CommandBuffer &) = delete;
    CommandBuffer &operator=(const CommandBuffer &) = delete;
    CommandBuffer(CommandBuffer &&commandBuffer) noexcept
    {
        mHandle = commandBuffer.mHandle;
        mCommandPool = commandBuffer.mCommandPool;
        mDevice = commandBuffer.mDevice;
        commandBuffer.mHandle = VK_NULL_HANDLE;
        commandBuffer.mDevice = VK_NULL_HANDLE;
        commandBuffer.mCommandPool = VK_NULL_HANDLE;
    }
    CommandBuffer &operator=(CommandBuffer &&commandBuffer) noexcept
    {
        vkFreeCommandBuffers(mDevice, mCommandPool, 1, &mHandle);
        mHandle = commandBuffer.mHandle;
        mCommandPool = commandBuffer.mCommandPool;
        mDevice = commandBuffer.mDevice;
        commandBuffer.mHandle = VK_NULL_HANDLE;
        commandBuffer.mDevice = VK_NULL_HANDLE;
        commandBuffer.mCommandPool = VK_NULL_HANDLE;
        return *this;
    }
    CommandBuffer(const Device &device, const CommandPool &commandPool, bool secondary = false)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        mDevice = device.GetHandle();
        mCommandPool = commandPool.GetHandle();

        VkCommandBufferAllocateInfo allocateInfo =
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = commandPool.GetHandle(),
                .level = (secondary) ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };

        VkCheck(vkAllocateCommandBuffers(device.GetHandle(), &allocateInfo, &mHandle));
    }
    ~CommandBuffer()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkFreeCommandBuffers(mDevice, mCommandPool, 1, &mHandle);
    }

    VkCommandBuffer GetHandle() const
    {
        return mHandle;
    }

    void BeginRecording(bool oneTimeSubmit = false, const VkCommandBufferInheritanceInfo &inheritanceInfo = {})
    {
        VkCommandBufferBeginInfo beginInfo =
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = (oneTimeSubmit) ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : (VkCommandBufferUsageFlags)0,
                .pInheritanceInfo = &inheritanceInfo,
            };
        VkCheck(vkBeginCommandBuffer(mHandle, &beginInfo));
    }

    void EndRecording()
    {
        vkEndCommandBuffer(mHandle);
    }

    void QueueSubmit(VkQueue queue, const Semaphore &waitSemaphore, const Semaphore &signalSemaphore, const Fence &fence, VkPipelineStageFlags waitStage)
    {
        // TODO: Fully implement this function

        VkSemaphore waitSemaphoreHandle = waitSemaphore.GetHandle();
        VkSemaphore signalSemaphoreHandle = signalSemaphore.GetHandle();

        VkSubmitInfo submitInfo =
            {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &waitSemaphoreHandle,
                .pWaitDstStageMask = &waitStage,
                .commandBufferCount = 1,
                .pCommandBuffers = &mHandle,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &signalSemaphoreHandle,
            };

        VkCheck(vkQueueSubmit(queue, 1, &submitInfo, fence.GetHandle()));
    }

    void QueueSubmit(VkQueue queue, bool wait = false)
    {

        VkSubmitInfo submitInfo =
            {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &mHandle,
            };

        VkCheck(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
        if (wait)
        {
            vkQueueWaitIdle(queue);
        }
    }

    void Reset()
    {
        vkResetCommandBuffer(mHandle, 0);
    }

private:
    VkCommandBuffer mHandle;
    VkDevice mDevice;
    VkCommandPool mCommandPool;
};

class RenderPass
{
public:
    RenderPass() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}
    RenderPass(const RenderPass &) = delete;
    RenderPass &operator=(const RenderPass &) = delete;

    RenderPass(RenderPass &&renderPass) noexcept
    {
        mHandle = renderPass.mHandle;
        mDevice = renderPass.mDevice;
        renderPass.mHandle = VK_NULL_HANDLE;
        renderPass.mDevice = VK_NULL_HANDLE;
    }
    RenderPass &operator=(RenderPass &&renderPass) noexcept
    {
        vkDestroyRenderPass(mDevice, mHandle, GetAllocationCallback());
        mHandle = renderPass.mHandle;
        mDevice = renderPass.mDevice;
        renderPass.mHandle = VK_NULL_HANDLE;
        renderPass.mDevice = VK_NULL_HANDLE;
        return *this;
    }
    RenderPass(const Device &device, const std::vector<VkSubpassDescription> &subpasses, const std::vector<VkSubpassDependency> &dependencies, const std::vector<VkAttachmentDescription> &attachments)
    {
        mDevice = device.GetHandle();
        VkRenderPassCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = (uint32_t)attachments.size(),
                .pAttachments = attachments.data(),
                .subpassCount = (uint32_t)subpasses.size(),
                .pSubpasses = subpasses.data(),
                .dependencyCount = (uint32_t)dependencies.size(),
                .pDependencies = dependencies.data(),
            };

        VkCheck(vkCreateRenderPass(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }
    ~RenderPass()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkDestroyRenderPass(mDevice, mHandle, GetAllocationCallback());
    }

    void CmdBeginRenderPass(const CommandBuffer &commandBuffer)
    {

        // TODO: Populate RenderPass begin info
        // TODO: Convert to using VkRenderPassBeginInfo2. msaa for depth pass can only be use once this struct is use
        VkRenderPassBeginInfo beginInfo =
            {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            };
        vkCmdBeginRenderPass(commandBuffer.GetHandle(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    // TODO: Add EndRenderPass

private:
    VkRenderPass mHandle;
    VkDevice mDevice;
};

class DescriptorPool
{
public:
    DescriptorPool() : mHandle(VK_NULL_HANDLE) {}
    DescriptorPool(const DescriptorPool &) = delete;
    DescriptorPool(DescriptorPool &&descriptorPool)
    {
        mHandle = descriptorPool.mHandle;
        mDevice = descriptorPool.mDevice;
        descriptorPool.mHandle = VK_NULL_HANDLE;
        descriptorPool.mDevice = VK_NULL_HANDLE;
    }
    DescriptorPool &operator=(const DescriptorPool &) = delete;
    DescriptorPool &operator=(DescriptorPool &&descriptorPool)
    {
        vkDestroyDescriptorPool(mDevice, mHandle, GetAllocationCallback());
        mHandle = descriptorPool.mHandle;
        mDevice = descriptorPool.mDevice;
        descriptorPool.mHandle = VK_NULL_HANDLE;
        descriptorPool.mDevice = VK_NULL_HANDLE;
        return *this;
    }
    DescriptorPool(const Device &device, const std::vector<VkDescriptorPoolSize> &pools)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        mDevice = device.GetHandle();
        uint32_t maxSets = 0;
        for (uint32_t i = 0; i < pools.size(); i++)
        {
            maxSets += pools[i].descriptorCount;
        }
        VkDescriptorPoolCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = maxSets,
                .poolSizeCount = (uint32_t)pools.size(),
                .pPoolSizes = pools.data(),
            };
        VkCheck(vkCreateDescriptorPool(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }

    ~DescriptorPool()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkDestroyDescriptorPool(mDevice, mHandle, GetAllocationCallback());
    }

    VkDescriptorPool GetHandle() const
    {
        return mHandle;
    }

private:
    VkDescriptorPool mHandle;
    VkDevice mDevice;
};

class DescriptorSetLayout
{
public:
    // TODO: Define move and copy constructor
    DescriptorSetLayout() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}
    DescriptorSetLayout(const Device &device, const std::vector<VkDescriptorSetLayoutBinding> &bindings)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        mDevice = device.GetHandle();
        VkDescriptorSetLayoutCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = (uint32_t)bindings.size(),
                .pBindings = bindings.data(),
            };

        VkCheck(vkCreateDescriptorSetLayout(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }
    ~DescriptorSetLayout()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkDestroyDescriptorSetLayout(mDevice, mHandle, GetAllocationCallback());
    }

    VkDescriptorSetLayout GetHandle() const
    {
        return mHandle;
    }

private:
    VkDescriptorSetLayout mHandle;
    VkDevice mDevice;
};

class Descriptor
{
public:
private:
    VkDescriptorSet mSet;
};

class Swapchain;

class Image
{
public:
    // TODO: Define move and copy constructor
    Image() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}
    Image(const Device &device, const glm::uvec3 &extent, VkFormat format, VkImageType imageType, VkImageUsageFlags usage, VkImageLayout initialLayout, VkSharingMode sharingMode, VkImageTiling tiling, VkSampleCountFlagBits sampleCount, uint32_t arrayLayers, uint32_t mipLevels)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        mDevice = device.GetHandle();
        VkImageCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = imageType,
                .format = format,
                .extent = {extent.x, extent.y, extent.z},
                .mipLevels = mipLevels,
                .arrayLayers = arrayLayers,
                .samples = sampleCount,
                .tiling = tiling,
                .usage = usage,
                .sharingMode = sharingMode,
                .initialLayout = initialLayout,
            };

        VkCheck(vkCreateImage(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }
    ~Image()
    {
        if (!mEnableDestructor)
            return;
        vkDestroyImage(mDevice, mHandle, GetAllocationCallback());
    }

    void Transition(const Device &device, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        CommandPool commandPool(device);

        CommandBuffer commandBuffer(device, commandPool);
        commandBuffer.BeginRecording(true);
        commandBuffer.EndRecording();
        commandBuffer.QueueSubmit(device.GetGraphicsQueue(), true);
    }

    void CmdTransition(const CommandBuffer &commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlagBits srcStageMask, VkPipelineStageFlagBits dstStageMask, VkDependencyFlagBits dependencyFlags, VkImageAspectFlags aspectMask, uint32_t baseLayer, uint32_t baseMipLevel, uint32_t layerCount, uint32_t levelCount)
    {

        VkImageMemoryBarrier imageBarrier =
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .image = mHandle,
                .subresourceRange =
                    {

                        .aspectMask = aspectMask,
                        .baseMipLevel = baseMipLevel,
                        .levelCount = levelCount,
                        .baseArrayLayer = baseLayer,
                        .layerCount = layerCount,
                    },
            };
        vkCmdPipelineBarrier(commandBuffer.GetHandle(), srcStageMask, dstStageMask, dependencyFlags, 0, nullptr, 0, nullptr, 1, &imageBarrier);
    }

    VkImage GetHandle() const
    {
        return mHandle;
    }

private:
    friend class Swapchain;
    Image(VkImage image, VkDevice device) : mHandle(image), mDevice(device), mEnableDestructor(false) {}
    VkImage mHandle;
    VkDevice mDevice;
    bool mEnableDestructor = true;
};

class ImageView
{
public:
    ImageView() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}
    ImageView(const ImageView &) = delete;
    ImageView &operator=(const ImageView &) = delete;
    ImageView(ImageView &&imageView)
    {
        mHandle = imageView.mHandle;
        mDevice = imageView.mDevice;
        imageView.mHandle = VK_NULL_HANDLE;
        imageView.mDevice = VK_NULL_HANDLE;
    }
    ImageView &operator=(ImageView &&imageView)
    {
        vkDestroyImageView(mDevice, mHandle, GetAllocationCallback());
        mHandle = imageView.mHandle;
        mDevice = imageView.mDevice;
        imageView.mHandle = VK_NULL_HANDLE;
        imageView.mDevice = VK_NULL_HANDLE;
        return *this;
    }
    ImageView(const Device &device, const Image &image, VkFormat format, VkImageViewType viewType, VkImageAspectFlags aspectMask, VkComponentMapping components, uint32_t layerCount, uint32_t mipmapLevelCount, uint32_t baseLayer, uint32_t baseMipLevel)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        mDevice = device.GetHandle();
        VkImageViewCreateInfo createInfo =
            {

                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = image.GetHandle(),
                .viewType = viewType,
                .format = format,
                .components = components,
                .subresourceRange =
                    {
                        .aspectMask = aspectMask,
                        .baseMipLevel = baseMipLevel,
                        .levelCount = mipmapLevelCount,
                        .baseArrayLayer = baseLayer,
                        .layerCount = layerCount,
                    },
            };

        VkCheck(vkCreateImageView(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }
    ~ImageView()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkDestroyImageView(mDevice, mHandle, GetAllocationCallback());
    }

private:
    VkImageView mHandle;
    VkDevice mDevice;
};

class Swapchain
{
public:
    // TODO: Define move and copy constructor
    // TODO: Define simplified constructor
    Swapchain() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}
    Swapchain(const Device &device, const Surface &surface, const glm::uvec2 &extent, VkFormat format, VkImageUsageFlags usage, VkColorSpaceKHR colorSpace, VkSharingMode sharingMode, VkCompositeAlphaFlagBitsKHR compositeAlpha, VkSurfaceTransformFlagBitsKHR preTransform, VkPresentModeKHR presentMode, uint32_t imageCount, uint32_t arrayLayer, bool clipped)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);
        assert(surface.GetHandle() != VK_NULL_HANDLE);

        mDevice = device.GetHandle();

        VkSwapchainCreateInfoKHR createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = surface.GetHandle(),
                .minImageCount = imageCount,
                .imageFormat = format,
                .imageColorSpace = colorSpace,
                .imageExtent = {extent.x, extent.y},
                .imageArrayLayers = arrayLayer,
                .imageUsage = usage,
                .imageSharingMode = sharingMode,
                .preTransform = preTransform,
                .compositeAlpha = compositeAlpha,
                .presentMode = presentMode,
                .clipped = clipped,
            };

        VkCheck(vkCreateSwapchainKHR(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));

        uint32_t currentImageCount = 0;
        vkGetSwapchainImagesKHR(device.GetHandle(), mHandle, &currentImageCount, nullptr);
        std::vector<VkImage> images(currentImageCount);
        vkGetSwapchainImagesKHR(device.GetHandle(), mHandle, &currentImageCount, images.data());

        for (VkImage image : images)
        {
            Image temp(image, device.GetHandle());
            const Image &ref = mImages.emplace_back(std::move(temp));

            mImageViews.emplace_back(device, ref, format, VK_IMAGE_VIEW_TYPE_2D,
                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                     VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
                                     1, 1, 0, 0);
        }
    }
    ~Swapchain()
    {
        if (mDevice == VK_NULL_HANDLE)
            return;
        vkDestroySwapchainKHR(mDevice, mHandle, GetAllocationCallback());
    }

    VkSwapchainKHR GetHandle() const
    {
        return mHandle;
    }

    const std::vector<Image> &GetImages() const
    {
        return mImages;
    }

    std::vector<Image> &GetImages()
    {
        return mImages;
    }

    const std::vector<ImageView> &GetImageViews() const
    {
        return mImageViews;
    }

    uint32_t GetNextImageIndex(const Semaphore &semaphore, const Fence &fence)
    {
        uint32_t index = 0;
        vkAcquireNextImageKHR(mDevice, mHandle, UINT64_MAX, semaphore.GetHandle(), fence.GetHandle(), &index);
        return index;
    }

private:
    VkSwapchainKHR mHandle;
    std::vector<Image> mImages;
    std::vector<ImageView> mImageViews;
    VkDevice mDevice;
};

class DescriptorSet
{
public:
    DescriptorSet() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}
    DescriptorSet(const Device &device, const DescriptorPool &descriptorPool, const DescriptorSetLayout &setLayout)
    {
        VkDescriptorSetLayout setLayoutHandle = setLayout.GetHandle();
        VkDescriptorSetAllocateInfo allocateInfo =
            {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = descriptorPool.GetHandle(),
                .descriptorSetCount = 1,
                .pSetLayouts = &setLayoutHandle,
            };

        mDevice = device.GetHandle();
        VkCheck(vkAllocateDescriptorSets(device.GetHandle(), &allocateInfo, &mHandle));
    }

    ~DescriptorSet()
    {
    }

private:
    VkDescriptorSet mHandle;
    VkDevice mDevice;
};

class ShaderModule
{
public:
    ShaderModule() : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE) {}

    ShaderModule(const Device &device, const std::vector<uint32_t> &code)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);

        VkShaderModuleCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = (uint32_t)code.size(),
                .pCode = code.data(),
            };

        mDevice = device.GetHandle();
        VkCheck(vkCreateShaderModule(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }

    ShaderModule(const Device &device, std::string_view filename)
        : mHandle(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE)
    {
        assert(device.GetHandle() != VK_NULL_HANDLE);

        FILE *fp = fopen(filename.data(), "rb");
        if (fp == nullptr)
        {
            std::println("Error: file not found {}", filename);
            return;
        }
        fseek(fp, 0L, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        std::vector<uint32_t> code(size);
        fread(code.data(), size, 1, fp);
        fclose(fp);

        VkShaderModuleCreateInfo createInfo =
            {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = (uint32_t)code.size(),
                .pCode = code.data(),
            };

        mDevice = device.GetHandle();
        VkCheck(vkCreateShaderModule(device.GetHandle(), &createInfo, GetAllocationCallback(), &mHandle));
    }

    ~ShaderModule()
    {
        vkDestroyShaderModule(mDevice, mHandle, GetAllocationCallback());
    }

    VkShaderModule GetHandle() const
    {
        return mHandle;
    }

private:
    VkShaderModule mHandle;
    VkDevice mDevice;
};

} // namespace vk

int main()
{
    vk::Window window(800, 600, "Raytracing");

    std::vector<const char *> instanceExtensions = window.GetInstanceExtensions();

    vk::Instance instance(instanceExtensions, {"VK_LAYER_KHRONOS_validation"});
    vk::PhysicalDevice physicalDevice(instance, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    vk::Surface surface(instance, window.GetHandle());
    vk::Device device(physicalDevice, {VK_KHR_SWAPCHAIN_EXTENSION_NAME}, {}, surface.GetHandle());
    vk::CommandPool commandPool(device);
    vk::CommandBuffer commandBuffer(device, commandPool);
    vk::DescriptorPool descriptorPool(device, {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}});

    VkDescriptorSetLayoutBinding storageImageBinding =
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };

    vk::DescriptorSetLayout descriptorSetLayout(device, {storageImageBinding});
    vk::Swapchain swapchain(device, surface, {800, 600}, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, VK_SHARING_MODE_EXCLUSIVE, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, VK_PRESENT_MODE_FIFO_KHR, 3, 1, VK_TRUE);

    vk::Semaphore imageAcquiredSemaphore(device);
    vk::Semaphore renderingFinishedSemaphore(device);
    vk::Fence imageAcquiredFence(device);

    vk::DescriptorSet descriptorSet(device, descriptorPool, descriptorSetLayout);

    vk::Image renderImage(device, {1920, 1080, 1}, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TYPE_2D,
                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_TILING_LINEAR, VK_SAMPLE_COUNT_1_BIT, 1, 1);

    renderImage.Transition(device, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    vk::ShaderModule computeShaderModule(device, "compute.comp.spv");

    // TODO: abstract Pipeline Layout into class
    // TODO: abstract Compute Pipeline into class

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        };

    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(device.GetHandle(), &pipelineLayoutCreateInfo, vk::GetAllocationCallback(), &pipelineLayout);

    VkComputePipelineCreateInfo createInfo =
        {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage =
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = computeShaderModule.GetHandle(),
                    .pName = "main",
                },
            .layout = pipelineLayout,
        };

    VkPipeline computePipeline;
    vkCreateComputePipelines(device.GetHandle(), VK_NULL_HANDLE, 1, &createInfo, vk::GetAllocationCallback(), &computePipeline);

    while (!window.ShouldClose())
    {
        glfwPollEvents();

        device.Wait();
        imageAcquiredFence.Wait();
        imageAcquiredFence.Reset();
        uint32_t imageIndex = swapchain.GetNextImageIndex(imageAcquiredSemaphore, imageAcquiredFence);
        commandBuffer.Reset();

        commandBuffer.BeginRecording();

        swapchain.GetImages()[imageIndex].CmdTransition(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlagBits)0, VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1, 1);

        vkCmdBindPipeline(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdDispatch(commandBuffer.GetHandle(), 16, 16, 1);

        swapchain.GetImages()[imageIndex].CmdTransition(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlagBits)0, VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1, 1);

        commandBuffer.EndRecording();

        commandBuffer.QueueSubmit(device.GetGraphicsQueue(), imageAcquiredSemaphore, renderingFinishedSemaphore, vk::Fence(), VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkSwapchainKHR swapchainHandle = swapchain.GetHandle();
        VkSemaphore waitPresentSemaphore = renderingFinishedSemaphore.GetHandle();

        VkPresentInfoKHR presentInfo =
            {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &waitPresentSemaphore,
                .swapchainCount = 1,
                .pSwapchains = &swapchainHandle,
                .pImageIndices = &imageIndex,
            };

        vkQueuePresentKHR(device.GetGraphicsQueue(), &presentInfo);
    }

    device.Wait();

    return 0;
}
