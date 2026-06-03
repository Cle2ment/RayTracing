-- xmake.lua — RayTracing project
set_project("RayTracing")
set_version("2.0")
set_xmakever("2.7.5")

add_rules("mode.debug", "mode.release", "mode.releasedbg")

-- ── Vulkan SDK ──
local vulkan_sdk = os.getenv("VULKAN_SDK")
if not vulkan_sdk then
    raise("VULKAN_SDK not set. Install Vulkan SDK 1.4+ and set VULKAN_SDK env var.")
end

-- ── Walnut (static library) ──
target("Walnut")
    set_kind("static")
    set_languages("c++17")

    -- Walnut framework source (headers auto-tracked, only .cpp needed)
    add_files("Walnut/Walnut/src/**.cpp")

    -- ImGui core + Vulkan/GLFW backends (only what Walnut uses)
    add_files(
        "Walnut/vendor/imgui/imgui.cpp",
        "Walnut/vendor/imgui/imgui_draw.cpp",
        "Walnut/vendor/imgui/imgui_tables.cpp",
        "Walnut/vendor/imgui/imgui_widgets.cpp",
        "Walnut/vendor/imgui/imgui_demo.cpp",
        "Walnut/vendor/imgui/backends/imgui_impl_vulkan.cpp",
        "Walnut/vendor/imgui/backends/imgui_impl_glfw.cpp",
        {public = false}
    )

    -- GLFW (Windows only — skip Posix/X11/Wayland/macOS sources)
    add_files(
        "Walnut/vendor/GLFW/src/context.c",
        "Walnut/vendor/GLFW/src/init.c",
        "Walnut/vendor/GLFW/src/input.c",
        "Walnut/vendor/GLFW/src/monitor.c",
        "Walnut/vendor/GLFW/src/platform.c",
        "Walnut/vendor/GLFW/src/vulkan.c",
        "Walnut/vendor/GLFW/src/window.c",
        "Walnut/vendor/GLFW/src/egl_context.c",
        "Walnut/vendor/GLFW/src/osmesa_context.c",
        "Walnut/vendor/GLFW/src/null_init.c",
        "Walnut/vendor/GLFW/src/null_joystick.c",
        "Walnut/vendor/GLFW/src/null_monitor.c",
        "Walnut/vendor/GLFW/src/null_window.c",
        "Walnut/vendor/GLFW/src/win32_init.c",
        "Walnut/vendor/GLFW/src/win32_joystick.c",
        "Walnut/vendor/GLFW/src/win32_module.c",
        "Walnut/vendor/GLFW/src/win32_monitor.c",
        "Walnut/vendor/GLFW/src/win32_thread.c",
        "Walnut/vendor/GLFW/src/win32_time.c",
        "Walnut/vendor/GLFW/src/win32_window.c",
        "Walnut/vendor/GLFW/src/wgl_context.c",
        {public = false}
    )

    -- Include paths
    add_includedirs(
        "Walnut/Walnut/src",
        "Walnut/vendor/imgui",
        "Walnut/vendor/imgui/backends",
        "Walnut/vendor/glfw/include",
        "Walnut/vendor/stb_image",
        "Walnut/vendor/glm",
        vulkan_sdk .. "/Include"
    )

    -- Vulkan link
    add_links("vulkan-1")
    add_linkdirs(vulkan_sdk .. "/Lib")

    -- Windows
    if is_plat("windows") then
        add_defines("WL_PLATFORM_WINDOWS", "_GLFW_WIN32", "_CRT_SECURE_NO_WARNINGS")
        add_links("Dwmapi", "opengl32", "gdi32", "user32", "kernel32", "shell32")
    end

    -- Config defines (matching premake5)
    add_defines("WL_DEBUG", { debug = true })
    add_defines("WL_RELEASE", { release = true })

-- ── RayTracing (executable) ──
target("RayTracing")
    set_kind("binary")
    set_languages("c++23")
    set_default(true)

    -- Source files
    add_files("RayTracing/src/**.cpp")

    -- Depend on Walnut
    add_deps("Walnut")

    -- Inherit Walnut's include paths
    add_includedirs(
        "Walnut/vendor/imgui",
        "Walnut/vendor/imgui/backends",
        "Walnut/vendor/glfw/include",
        "Walnut/vendor/glm",
        "Walnut/Walnut/src",
        vulkan_sdk .. "/Include"
    )

    if is_plat("windows") then
        add_cxflags("/utf-8", "/EHsc")
        add_defines("WL_PLATFORM_WINDOWS")
        add_links("opengl32", "gdi32")
    end

    -- Config defines
    add_defines("WL_DEBUG", { debug = true })
    add_defines("WL_RELEASE", { release = true })
