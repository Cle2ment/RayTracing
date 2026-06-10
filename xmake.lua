-- xmake.lua — RayTracing project
set_project("RayTracing")
set_version("2.0")
set_xmakever("2.7.5")

add_rules("mode.debug", "mode.release", "mode.releasedbg")

-- ── External packages ──
add_requires("glm 1.0.3")
add_requires("stb 2026.03.18")
add_requires("glfw 3.4")

-- ── Vulkan SDK ──
local vulkan_sdk = os.getenv("VULKAN_SDK")
if not vulkan_sdk then
    raise("VULKAN_SDK not set. Install Vulkan SDK 1.4+ and set VULKAN_SDK env var.")
end

-- ── Peanut (static library) ──
target("Peanut")
    set_kind("static")
    set_languages("c++17")
    add_packages("glm")
    add_packages("stb")

    add_packages("glfw")

    -- Peanut framework source (headers auto-tracked, only .cpp needed)
    add_files("Peanut/Peanut/src/**.cpp")

    -- ImGui core + Vulkan/GLFW backends (only what Peanut uses)
    add_files(
        "Peanut/vendor/imgui/imgui.cpp",
        "Peanut/vendor/imgui/imgui_draw.cpp",
        "Peanut/vendor/imgui/imgui_tables.cpp",
        "Peanut/vendor/imgui/imgui_widgets.cpp",
        "Peanut/vendor/imgui/imgui_demo.cpp",
        "Peanut/vendor/imgui/backends/imgui_impl_vulkan.cpp",
        "Peanut/vendor/imgui/backends/imgui_impl_glfw.cpp",
        {public = false}
    )

    -- Include paths
    add_includedirs(
        "Peanut/Peanut/src",
        "Peanut/vendor/imgui",
        "Peanut/vendor/imgui/backends",
        vulkan_sdk .. "/Include"
    )

    -- Vulkan link
    add_links("vulkan-1")
    add_linkdirs(vulkan_sdk .. "/Lib")

    -- Windows
    if is_plat("windows") then
        add_defines("PN_PLATFORM_WINDOWS", "_GLFW_WIN32", "_CRT_SECURE_NO_WARNINGS")
        add_links("Dwmapi", "opengl32", "gdi32", "user32", "kernel32", "shell32")
    end

    -- Config defines
    add_defines("PN_DEBUG", { debug = true })
    add_defines("PN_RELEASE", { release = true })

-- ── RayTracing (executable) ──
target("RayTracing")
    set_kind("binary")
    set_languages("c++23")
    set_default(true)

    -- Source files
    add_files("RayTracing/src/**.cpp")

    -- Depend on Peanut
    add_deps("Peanut")
    add_packages("glfw")

    -- Inherit Peanut's include paths
    add_packages("glm")
    add_packages("stb")
    add_defines("GLM_ENABLE_EXPERIMENTAL")
    add_includedirs(
        "Peanut/vendor/imgui",
        "Peanut/vendor/imgui/backends",
        "Peanut/Peanut/src",
        vulkan_sdk .. "/Include"
    )

    -- Warnings as errors for MSVC: /W4 = equivalent to -Wall -Wextra
    -- Suppress: C4100 (unused param, common in vendor callbacks), C4062 (unhandled enum, noise)
    if is_plat("windows") then
        add_cxflags("/utf-8", "/EHsc", "/W4", "/wd4100", "/wd4062")
        add_defines("PN_PLATFORM_WINDOWS", "NOMINMAX")
        add_links("opengl32", "gdi32")
    end

    -- Config defines
    add_defines("PN_DEBUG", { debug = true })
    add_defines("PN_RELEASE", { release = true })

-- ── CUDA Support ──
local cuda_path = os.getenv("CUDA_PATH")
local cuda_found = cuda_path and os.isdir(cuda_path)

if cuda_found then
    target("RayTracing")
        add_defines("PN_CUDA")
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
        add_defines("PN_ISPC")

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
        add_defines("PN_OPTIX", "NOMINMAX")
        add_files("RayTracing/src/OptiXDenoiser.cpp")
        add_includedirs(optix_include)
        add_links("cuda", "Advapi32")
end

-- ── Unit Tests (Catch2 v2, single-header vendored) ──
target("RayTracing_test")
    set_kind("binary")
    set_languages("c++23")
    add_packages("glm")
    add_files("RayTracing/test/**.cpp")
    add_includedirs("RayTracing/src", "vendor/catch2")
