VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
LibraryDir = {}
Library = {}

-- Include Directories
	IncludeDir["glfw"] = "%{wks.location}/MinecraftRecoded/Dependencies/glfw-3.3.7/include/"
	IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include/"

-- Library Directories
	LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib/"

-- Libraries
	Library["VulkanSDK"] = "vulkan-1"
