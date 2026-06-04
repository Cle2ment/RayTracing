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
        add_defines("WL_PLATFORM_WINDOWS", "NOMINMAX")
        add_links("opengl32", "gdi32")
    end

    -- Config defines
    add_defines("WL_DEBUG", { debug = true })
    add_defines("WL_RELEASE", { release = true })

-- ── CUDA Support ──
local cuda_path = os.getenv("CUDA_PATH")
local cuda_found = cuda_path and os.isdir(cuda_path)

if cuda_found then
    target("RayTracing")
        add_defines("WL_CUDA")
        add_files("RayTracing/src/CUDARenderer.cu")
        add_includedirs(cuda_path .. "/include", "RayTracing/src")
        add_linkdirs(cuda_path .. "/lib/x64")
        add_links("cudart")

        -- CUDA architecture targets
        add_cugencodes("compute_75", "sm_75")
        add_cugencodes("compute_86", "sm_86")
        add_cugencodes("compute_89", "sm_89")
        add_cugencodes("compute_120", "sm_120")

        -- NVCC flags
        add_cuflags("--use_fast_math")
        if is_mode("release") then
            add_cuflags("-lineinfo")  -- Debug mode uses -G (device-debug) which implies lineinfo
        end
end

-- ── ISPC Support (CPU SIMD acceleration) ──
local ispc_path = "vendor/ispc/bin/ispc.exe"
local ispc_found = os.isfile(ispc_path)

if ispc_found then
    target("RayTracing")
        add_defines("WL_ISPC")

        before_build(function (target)
            local ispc = path.absolute("vendor/ispc/bin/ispc.exe")
            local src  = path.absolute("RayTracing/src/PathTracer.ispc")
            local outdir = path.absolute("RayTracing/src")
            os.runv(ispc, {
                "--target=avx2",
                "--arch=x86-64", "--opt=disable-assertions",
                src,
                "-o", path.join(outdir, "PathTracer.ispc.obj"),
                "-h", path.join(outdir, "PathTracer_ispc.h")
            })
        end)

        add_includedirs("RayTracing/src")
        add_ldflags(path.absolute("RayTracing/src/PathTracer.ispc.obj"), {force = true})
end

-- ── OptiX Denoiser Support ──
local optix_found = false
local optix_include = nil

-- Check OptiX_ROOT / OPTIX_PATH environment variables
local optix_env = os.getenv("OptiX_ROOT") or os.getenv("OPTIX_PATH")
if optix_env then
    local inc = path.join(optix_env, "include")
    if os.isdir(inc) then
        optix_include = inc
        optix_found = true
    end
end

-- Fallback: scan ProgramData for OptiX SDK installations
if not optix_found then
    local base = "C:/ProgramData/NVIDIA Corporation"
    if os.isdir(base) then
        for _, p in ipairs(os.dirs(base .. "/*")) do
            if p:find("OptiX SDK") then
                local inc = path.join(p, "include")
                if os.isdir(inc) then
                    optix_include = inc
                    optix_found = true
                    break
                end
            end
        end
    end
end

if optix_found and cuda_found then
    target("RayTracing")
        add_defines("WL_OPTIX", "NOMINMAX")
        add_files("RayTracing/src/OptiXDenoiser.cpp")
        add_includedirs(optix_include)
        add_links("cuda", "Advapi32")
end
