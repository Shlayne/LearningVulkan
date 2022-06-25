#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <vector>

namespace core
{
	static constexpr int32_t WINDOW_WIDTH = 1280;
	static constexpr int32_t WINDOW_HEIGHT = 720;
	static constexpr const char WINDOW_TITLE[] = "Minecraft Recoded";

	class Application
	{
	public:
		Application();
		~Application();
	public:
		void Run();
	private:

	private:
		GLFWwindow* m_pWindow;

		VkInstance m_pInstance = VK_NULL_HANDLE;
#if !CONFIG_DIST // ENABLE_LOGGING
		VkDebugUtilsMessengerEXT m_pDebugMessenger = VK_NULL_HANDLE;
#endif
		VkPhysicalDevice m_pPhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_pDevice = VK_NULL_HANDLE;
		VkQueue m_pGraphicsQueue = VK_NULL_HANDLE;
		VkSurfaceKHR m_pSurface = VK_NULL_HANDLE;
		VkQueue m_pPresentQueue = VK_NULL_HANDLE;
		VkSwapchainKHR m_pSwapChain = VK_NULL_HANDLE;
		VkFormat m_SwapChainFormat = VK_FORMAT_UNDEFINED;
		VkExtent2D m_SwapChainExtent{};
		std::vector<VkImage> m_SwapChainImages;
		std::vector<VkImageView> m_SwapChainImageViews;
	};
}
