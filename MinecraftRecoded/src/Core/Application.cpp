#include "Core/Application.h"
#include <assert.h>
#include <iostream>
#include <unordered_map>
#include <optional>
#include <unordered_set>
#include <array>
#include <algorithm>

static std::unordered_map<const char*, PFN_vkVoidFunction> s_VulkanExtensionFunctions;

template<typename PFN, typename Ret, typename... Args>
static Ret vkCallFunctionEXT_IMPL(VkInstance pInstance, const char* cpFunctionName, Args&&... args)
{
	PFN_vkVoidFunction fFunction;

	auto it = s_VulkanExtensionFunctions.find(cpFunctionName);
	if (it != s_VulkanExtensionFunctions.end())
		fFunction = it->second;
	else
	{
		fFunction = vkGetInstanceProcAddr(pInstance, cpFunctionName);
		s_VulkanExtensionFunctions[cpFunctionName] = fFunction;
	}

	if (fFunction == nullptr)
		return static_cast<Ret>(VK_ERROR_EXTENSION_NOT_PRESENT);
	return static_cast<Ret>(((PFN)fFunction)(pInstance, std::forward<Args>(args)...));
}

#define vkCallFunctionEXT(func, ...) vkCallFunctionEXT_IMPL<PFN_##func, VkResult>(m_pInstance, #func __VA_OPT__(,) __VA_ARGS__)
#define vkCallVoidFunctionEXT(func, ...) vkCallFunctionEXT_IMPL<PFN_##func, void>(m_pInstance, #func __VA_OPT__(,) __VA_ARGS__)

#if !CONFIG_DIST // !ENABLE_LOGGING
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessageCallback
(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* cpCallbackData,
	void* pUserData
)
{
	// VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT -> trace
	// VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT -> info
	// VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT -> warning
	// VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT -> error
	std::cerr << "Vulkan Debug: " << cpCallbackData->pMessage << '\n';
	return VK_FALSE;
}
#endif

namespace core
{
	Application::Application()
	{
		// Initialize GLFW and create a window.
		{
			int32_t glfwInitialized = glfwInit();
			assert(glfwInitialized && "Failed to initialize GLFW.");

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

			m_pWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
			assert(m_pWindow && "Failed to create window.");
		}

		// Setup Vulkan.
		{
			VkResult result = VK_SUCCESS;

			// Get required extensions
			uint32_t glfwRequiredExtensionCount;
			const char** cppGLFWRequiredExtensions = glfwGetRequiredInstanceExtensions(&glfwRequiredExtensionCount);
			assert(cppGLFWRequiredExtensions && "Failed to get required glfw extensions.");
			std::vector<const char*> requiredExtensions(cppGLFWRequiredExtensions, cppGLFWRequiredExtensions + glfwRequiredExtensionCount);
#if !CONFIG_DIST // ENABLE_LOGGING
			requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

			// Get info.
			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = WINDOW_TITLE;
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "Minecraft Recoded Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_3;

			VkInstanceCreateInfo instanceCreateInfo{};
			instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instanceCreateInfo.pApplicationInfo = &appInfo;
			instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
			instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();

			// Validation Layers for debugging.
#if !CONFIG_DIST // ENABLE_LOGGING.
			uint32_t layerCount;
			result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
			assert(result == VK_SUCCESS && "Failed to get available layer count.");
			std::vector<VkLayerProperties> availableLayerProperties(layerCount);
			result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayerProperties.data());
			assert(result == VK_SUCCESS && "Failed to get available layer properties.");

			constexpr auto validationLayers = std::to_array({
				"VK_LAYER_KHRONOS_validation"
			});

			const char* cpLayerNotFound = nullptr;
			for (const char* cpLayerName : validationLayers)
			{
				bool layerFound = false;
				for (const VkLayerProperties& crLayerProperties : availableLayerProperties)
				{
					if (strcmp(cpLayerName, crLayerProperties.layerName) == 0)
					{
						layerFound = true;
						break;
					}
				}

				if (!layerFound)
				{
					cpLayerNotFound = cpLayerName;
					break;
				}
			}

			assert(cpLayerNotFound == nullptr && "Failed to find validation layer.");

			instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
#endif

#if !CONFIG_DIST // ENABLE_LOGGING
			// Let the debug messenger callback be used for creating and destroying the instance.
			VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
			debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugMessengerCreateInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |*/
				/*VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugMessengerCreateInfo.pfnUserCallback = DebugMessageCallback;

			instanceCreateInfo.pNext = &debugMessengerCreateInfo;
#endif

			// Create the instance.
			result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_pInstance);
			assert(result == VK_SUCCESS && "Failed to create Vulkan instance.");

#if !CONFIG_DIST // ENABLE_LOGGING
			// Create the debug messenger.
			result = vkCallFunctionEXT(vkCreateDebugUtilsMessengerEXT, &debugMessengerCreateInfo, nullptr, &m_pDebugMessenger);
			assert(result == VK_SUCCESS && "Failed to create Vulkan debug messenger.");
#endif

			// Create window surface for rendering to the glfw window from Vulkan.
			result = glfwCreateWindowSurface(m_pInstance, m_pWindow, nullptr, &m_pSurface);
			assert(result == VK_SUCCESS && "Failed to create window surface.");

			// Select a suitable physical device. (for future reference, you can use multiple physical devices simultaneously)
			uint32_t physicalDeviceCount;
			result = vkEnumeratePhysicalDevices(m_pInstance, &physicalDeviceCount, nullptr);
			assert(result == VK_SUCCESS && physicalDeviceCount > 0 && "Failed to get physical device count.");
			std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
			result = vkEnumeratePhysicalDevices(m_pInstance, &physicalDeviceCount, physicalDevices.data());
			assert(result == VK_SUCCESS && "Failed to get physical devices.");

			constexpr auto requiredDeviceExtensions = std::to_array({
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			});

			struct QueueFamilyIndices
			{
				std::optional<uint32_t> graphics;
				std::optional<uint32_t> present;

				constexpr bool IsComplete() const noexcept
				{
					return graphics.has_value() && present.has_value();
				}

				std::unordered_set<uint32_t> GetUniqueIndices() const noexcept
				{
					assert(IsComplete() && "Tried to get unique indices for an incomplete set of queue family indices.");
					return { graphics.value(), present.value() };
				}

				constexpr auto GetIndices() const noexcept
				{
					assert(IsComplete() && "Tried to get indices for an incomplete set of queue family indices.");
					return std::to_array({ graphics.value(), present.value() });
				}
			};

			QueueFamilyIndices queueFamilyIndices;
			VkSurfaceCapabilitiesKHR swapChainCapabilities;
			VkSurfaceFormatKHR swapChainSurfaceFormat;
			VkPresentModeKHR swapChainPresentMode;
			VkExtent2D swapChainExtent;
			{
				VkPhysicalDeviceProperties physicalDeviceProperties;
				VkPhysicalDeviceFeatures physicalDeviceFeatures;
				for (VkPhysicalDevice pPhysicalDevice : physicalDevices)
				{
					vkGetPhysicalDeviceProperties(pPhysicalDevice, &physicalDeviceProperties);
					vkGetPhysicalDeviceFeatures(pPhysicalDevice, &physicalDeviceFeatures);

					// Check if the device is a dedicated GPU.
					if (physicalDeviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
						continue;

					// Check if the device has required queue families.
					{
						uint32_t queueFamilyCount;
						vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice, &queueFamilyCount, nullptr);
						std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
						vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevice, &queueFamilyCount, queueFamilies.data());

						QueueFamilyIndices pendingQueueFamilyIndices;
						for (uint32_t i = 0; i < queueFamilyCount && !pendingQueueFamilyIndices.IsComplete(); i++)
						{
							const VkQueueFamilyProperties& queueFamily = queueFamilies[i];

							if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
								pendingQueueFamilyIndices.graphics = i;

							VkBool32 presentSupport = VK_FALSE;
							result = vkGetPhysicalDeviceSurfaceSupportKHR(pPhysicalDevice, i, m_pSurface, &presentSupport);
							assert(result == VK_SUCCESS && "Failed to query present support.");
							if (presentSupport == VK_TRUE)
								pendingQueueFamilyIndices.present = i;
						}

						if (!pendingQueueFamilyIndices.IsComplete())
							continue;

						queueFamilyIndices = pendingQueueFamilyIndices;
					}

					// Check if the device has the required extensions.
					{
						uint32_t extensionCount;
						result = vkEnumerateDeviceExtensionProperties(pPhysicalDevice, nullptr, &extensionCount, nullptr);
						assert(result == VK_SUCCESS && "Failed to get physical device extension count.");
						std::vector<VkExtensionProperties> availableExtensions(extensionCount);
						result = vkEnumerateDeviceExtensionProperties(pPhysicalDevice, nullptr, &extensionCount, availableExtensions.data());
						assert(result == VK_SUCCESS && "Failed to get physical device extensions.");

						bool hasRequiredExtensions = true;
						for (const char* cpRequiredExtension : requiredDeviceExtensions)
						{
							auto it = std::find_if(availableExtensions.begin(), availableExtensions.end(),
								[cpRequiredExtension](const VkExtensionProperties& crAvailableExtension)
								{
									return strcmp(crAvailableExtension.extensionName, cpRequiredExtension) == 0;
								}
							);

							if (it == availableExtensions.end())
							{
								hasRequiredExtensions = false;
								break;
							}
						}

						if (!hasRequiredExtensions)
							continue;
					}

					// Check if the swap chain has required support.
					{
						VkSurfaceCapabilitiesKHR surfaceCapabilities;
						result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pPhysicalDevice, m_pSurface, &surfaceCapabilities);
						assert(result == VK_SUCCESS && "Failed to get physical device surface capabilities.");

						uint32_t formatCount;
						result = vkGetPhysicalDeviceSurfaceFormatsKHR(pPhysicalDevice, m_pSurface, &formatCount, nullptr);
						assert(result == VK_SUCCESS && "Failed to get physical device surface format count.");
						std::vector<VkSurfaceFormatKHR> formats(formatCount);
						result = vkGetPhysicalDeviceSurfaceFormatsKHR(pPhysicalDevice, m_pSurface, &formatCount, formats.data());
						assert(result == VK_SUCCESS && "Failed to get physical device surface formats.");

						uint32_t presentModeCount;
						result = vkGetPhysicalDeviceSurfacePresentModesKHR(pPhysicalDevice, m_pSurface, &presentModeCount, nullptr);
						assert(result == VK_SUCCESS && "Failed to get physical device surface present mode count.");
						std::vector<VkPresentModeKHR> presentModes(presentModeCount);
						result = vkGetPhysicalDeviceSurfacePresentModesKHR(pPhysicalDevice, m_pSurface, &presentModeCount, presentModes.data());
						assert(result == VK_SUCCESS && "Failed to get physical device surface present modes.");

						// This check won't always be the case. Certain capabilities, formats, present modes, and  may be required.
						if (formats.empty() || presentModes.empty())
							continue;

						// Get swap chain info.

						swapChainCapabilities = surfaceCapabilities;

						// VK_FORMAT_B8G8R8A8_SRGB and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR are preferred,
						// but if no such format exists, default to the first format.
						swapChainSurfaceFormat = formats.front();
						for (const VkSurfaceFormatKHR& crFormat : formats)
						{
							if (crFormat.format == VK_FORMAT_B8G8R8A8_SRGB && crFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
							{
								swapChainSurfaceFormat = crFormat;
								break;
							}
						}

						// VK_PRESENT_MODE_FIFO_KHR is always gaurenteed to exist.
						// However, some games, Terraria for example, allow frame skipping,
						// which I THINK is VK_PRESENT_MODE_MAILBOX_KHR.
						swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
						//bool wantFrameSkip = false;
						//if (wantFrameSkip)
						//{
						//	for (const VkPresentModeKHR& crPresentMode : presentModes)
						//	{
						//		if (crPresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
						//		{
						//			presentMode = crPresentMode;
						//			break;
						//		}
						//	}
						//}
						//if (wantFrameSkip && presentMode != VK_PRESENT_MODE_MAILBOX_KHR)
						//	; // warn: could not enable frame skip.

						swapChainExtent = surfaceCapabilities.currentExtent;
						if (swapChainExtent.width == UINT32_MAX || swapChainExtent.height == UINT32_MAX)
						{
							glfwGetFramebufferSize(m_pWindow, (int32_t*)(&swapChainExtent.width), (int32_t*)(&swapChainExtent.height));
							swapChainExtent.width = std::clamp(swapChainExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
							swapChainExtent.height = std::clamp(swapChainExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
						}
					}

					m_pPhysicalDevice = pPhysicalDevice;
					break;
				}

				assert(m_pPhysicalDevice != VK_NULL_HANDLE && "Failed to find suitable physical device.");
			}

			// Create the logical device.
			{
				// Create device queue infos.
				constexpr float queuePriority = 1.0f;
				std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
				std::unordered_set<uint32_t> uniqueQueueFamilyIndices = queueFamilyIndices.GetUniqueIndices();
				for (uint32_t queueFamilyIndex : uniqueQueueFamilyIndices)
				{
					VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
					deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
					deviceQueueCreateInfo.queueCount = 1;
					deviceQueueCreateInfo.pQueuePriorities = &queuePriority;
					deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
				}

				VkPhysicalDeviceFeatures deviceFeatures{};

				// Create the logical device info.
				VkDeviceCreateInfo deviceCreateInfo{};
				deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
				deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
				deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
				deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
				deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
				deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

#if !CONFIG_DIST // ENABLE_LOGGING.
				// These two are only for backwards compatability. They are ignored by newer versions of Vulkan.
				deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
				deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
#endif

				result = vkCreateDevice(m_pPhysicalDevice, &deviceCreateInfo, nullptr, &m_pDevice);
				assert(result == VK_SUCCESS && "Failed to create logical device.");

				// Get device queue handles.
				vkGetDeviceQueue(m_pDevice, queueFamilyIndices.graphics.value(), 0, &m_pGraphicsQueue);
				vkGetDeviceQueue(m_pDevice, queueFamilyIndices.present.value(), 0, &m_pPresentQueue);
			}

			// Create swap chain.
			{
				// Get the image count.
				uint32_t imageCount = swapChainCapabilities.minImageCount + 1;
				if (swapChainCapabilities.maxImageCount > 0 && imageCount > swapChainCapabilities.maxImageCount)
					imageCount = swapChainCapabilities.maxImageCount;

				VkSwapchainCreateInfoKHR swapChainCreateInfo{};
				swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
				swapChainCreateInfo.surface = m_pSurface;
				swapChainCreateInfo.minImageCount = imageCount;
				swapChainCreateInfo.imageFormat = swapChainSurfaceFormat.format;
				swapChainCreateInfo.imageColorSpace = swapChainSurfaceFormat.colorSpace;
				swapChainCreateInfo.imageExtent = swapChainExtent;
				swapChainCreateInfo.imageArrayLayers = 1;
				swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

				auto indices = queueFamilyIndices.GetIndices();
				if (queueFamilyIndices.graphics != queueFamilyIndices.present)
				{
					swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
					swapChainCreateInfo.queueFamilyIndexCount = 2;
					swapChainCreateInfo.pQueueFamilyIndices = indices.data();
				}
				else
					swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

				swapChainCreateInfo.preTransform = swapChainCapabilities.currentTransform;
				swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				swapChainCreateInfo.presentMode = swapChainPresentMode;
				swapChainCreateInfo.clipped = VK_TRUE;
				swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

				result = vkCreateSwapchainKHR(m_pDevice, &swapChainCreateInfo, nullptr, &m_pSwapChain);
				assert(result == VK_SUCCESS && "Failed to create swap chain.");

				m_SwapChainFormat = swapChainSurfaceFormat.format;
				m_SwapChainExtent = swapChainExtent;

				// Get swap chain image handles.
				uint32_t swapChainImageCount;
				result = vkGetSwapchainImagesKHR(m_pDevice, m_pSwapChain, &swapChainImageCount, nullptr);
				assert(result == VK_SUCCESS && "Failed to get swap chain image count.");
				m_SwapChainImages.resize(swapChainImageCount);
				result = vkGetSwapchainImagesKHR(m_pDevice, m_pSwapChain, &swapChainImageCount, m_SwapChainImages.data());
				assert(result == VK_SUCCESS && "Failed to get swap chain images.");
			}

			// Create swap chain image views
			{
				VkImageViewCreateInfo imageViewCreateInfo{};
				imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				imageViewCreateInfo.format = swapChainSurfaceFormat.format;
				imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
				imageViewCreateInfo.subresourceRange.levelCount = 1;
				imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
				imageViewCreateInfo.subresourceRange.layerCount = 1;

				m_SwapChainImageViews.resize(m_SwapChainImages.size());
				for (size_t i = 0; i < m_SwapChainImages.size(); i++)
				{
					imageViewCreateInfo.image = m_SwapChainImages[i];
					result = vkCreateImageView(m_pDevice, &imageViewCreateInfo, nullptr, &m_SwapChainImageViews[i]);
					assert(result == VK_SUCCESS && "Failed to create swap chain image view.");
				}
			}

			// Create graphics pipeline.
			{
				// https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules
				// https://github.com/Shlayne/MinecraftRecoded
				// https://github.com/TheCherno/Walnut/blob/master/Walnut/src/Walnut/Application.cpp
			}
		}
	}

	Application::~Application()
	{
		for (VkImageView pImageView : m_SwapChainImageViews)
			vkDestroyImageView(m_pDevice, pImageView, nullptr);
		vkDestroySwapchainKHR(m_pDevice, m_pSwapChain, nullptr);
		vkDestroyDevice(m_pDevice, nullptr);
		vkDestroySurfaceKHR(m_pInstance, m_pSurface, nullptr);
#if !CONFIG_DIST // ENABLE_LOGGING
		vkCallVoidFunctionEXT(vkDestroyDebugUtilsMessengerEXT, m_pDebugMessenger, nullptr);
#endif
		vkDestroyInstance(m_pInstance, nullptr);

		glfwDestroyWindow(m_pWindow);
		glfwTerminate();
	}

	void Application::Run()
	{
		while (!glfwWindowShouldClose(m_pWindow))
		{
			glfwPollEvents();
		}
	}
}
