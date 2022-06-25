project "LearningVulkan"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	cdialect "C17"
	staticruntime "On"

	targetdir ("%{wks.location}/bin/" .. OutputDir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. OutputDir .. "/%{prj.name}")

	files {
		"src/**.h",
		"src/**.cpp",
		"src/**.inl"
	}

	includedirs {
		-- Add any project source directories here.
		"src",
		-- "%{wks.location}/__PROJECT_NAME__/src",

		-- Add any dependency includes here.
		"%{IncludeDir.glfw}",
		"%{IncludeDir.VulkanSDK}",
	}

	libdirs {
		"%{LibraryDir.VulkanSDK}"
	}

	-- Add any links dependency libs via their project names here.
	links {
		"glfw",
		"%{Library.VulkanSDK}",
	}

	filter "system:windows"
		systemversion "latest"
		usestdpreproc "On"
		buildoptions "/wd5105" -- Until Microsoft updates Windows 10 to not have terrible code (aka never), this must be here to prevent a warning.
		defines "SYSTEM_WINDOWS"

	filter "configurations:Profile"
		runtime "Debug"
		optimize "Off"
		symbols "On"
		defines "CONFIG_PROFILE"

	filter "configurations:Debug"
		runtime "Debug"
		optimize "Debug"
		symbols "Full"
		defines "CONFIG_DEBUG"

	filter "configurations:Release"
		runtime "Release"
		optimize "On"
		symbols "On"
		defines "CONFIG_RELEASE"

	filter "configurations:Dist"
		kind "WindowedApp"
		runtime "Release"
		optimize "Full"
		symbols "Off"
		defines "CONFIG_DIST"
