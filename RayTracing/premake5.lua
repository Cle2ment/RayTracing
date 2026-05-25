-- CUDA Toolkit Detection (environment variable only, no hardcoded paths)
local cudaPath = os.getenv("CUDA_PATH")
   or os.getenv("CUDA_PATH_V13_2")
   or os.getenv("CUDA_PATH_V12_8")
   or os.getenv("CUDA_PATH_V12_6")
   or os.getenv("CUDA_PATH_V12_5")
   or os.getenv("CUDA_PATH_V12_4")
   or os.getenv("CUDA_PATH_V11_8")
local cudaFound = cudaPath ~= nil and os.isdir(cudaPath)

if not cudaFound then
   print("NOTE: CUDA_PATH not set or invalid. Building CPU-only path tracer.")
   print("      To enable GPU acceleration, set CUDA_PATH to your CUDA Toolkit directory.")
   print("      Example: setx CUDA_PATH \"C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v13.2\"")
end

-- CUDA Architecture Targets (virtual + real)
-- sm_75: Turing (RTX 20xx, GTX 16xx)
-- sm_86: Ampere (RTX 30xx)
-- sm_89: Ada Lovelace (RTX 40xx)
local cudaArchs = {
   "compute_75,code=sm_75",
   "compute_86,code=sm_86",
   "compute_89,code=sm_89",
}

function getCudaArchFlags()
   local flags = ""
   for _, arch in ipairs(cudaArchs) do
      flags = flags .. " -gencode=arch=" .. arch
   end
   return flags
end

project "RayTracing"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files { "src/**.h", "src/**.cpp" }
   if cudaFound then
      files { "src/**.cuh" }
   end

   includedirs
   {
      "../Walnut/vendor/imgui",
      "../Walnut/vendor/glfw/include",
      "../Walnut/vendor/glm",

      "../Walnut/Walnut/src",

      "%{IncludeDir.VulkanSDK}",
   }

   links
   {
       "Walnut"
   }

   targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   -- CUDA Configuration
   if cudaFound then
      defines { "WL_CUDA" }
      includedirs { cudaPath .. "/include" }
      libdirs { cudaPath .. "/lib/x64" }
      links { "cudart" }

      -- Explicitly add NVCC output .obj to linker inputs
      linkoptions { '"$(IntDir)CUDARenderer.obj"' }

      -- CUDA architecture flags
      local cudaArchFlags = getCudaArchFlags()

      -- Pre-build step: compile .cu files with NVCC → .obj in $(IntDir)
      -- Linker automatically picks up all .obj files from $(IntDir)
      local nvccCmd = '"' .. cudaPath .. '/bin/nvcc"'
         .. ' -ccbin="$(VCToolsInstallDir)bin\\Hostx64\\x64"'
         .. ' -lineinfo --use_fast_math'
         .. ' -I"' .. cudaPath .. '/include"'
         .. ' -I"' .. path.getabsolute("../Walnut/vendor/imgui") .. '"'
         .. ' -I"' .. path.getabsolute("../Walnut/vendor/glfw/include") .. '"'
         .. ' -I"' .. path.getabsolute("../Walnut/vendor/glm") .. '"'
         .. ' -I"' .. path.getabsolute("../Walnut/Walnut/src") .. '"'
         .. ' -I"%{IncludeDir.VulkanSDK}"'
         .. ' -DWL_PLATFORM_WINDOWS -DWL_CUDA'
         .. ' ' .. cudaArchFlags

      filter "configurations:Debug"
         prebuildcommands {
            nvccCmd
            .. ' -Xcompiler "/EHsc,/W3,/nologo,/Zi,/MDd"'
            .. ' -G -DWL_DEBUG'
            .. ' --compile'
            .. ' -o "$(IntDir)CUDARenderer.obj"'
            .. ' "src\\CUDARenderer.cu"'
         }

      filter "configurations:Release"
         prebuildcommands {
            nvccCmd
            .. ' -Xcompiler "/EHsc,/W3,/nologo,/Zi,/MD"'
            .. ' -O2 -DWL_RELEASE'
            .. ' --compile'
            .. ' -o "$(IntDir)CUDARenderer.obj"'
            .. ' "src\\CUDARenderer.cu"'
         }

      filter "configurations:Dist"
         prebuildcommands {
            nvccCmd
            .. ' -Xcompiler "/EHsc,/W3,/nologo,/MD"'
            .. ' -O2 -DWL_DIST'
            .. ' --compile'
            .. ' -o "$(IntDir)CUDARenderer.obj"'
            .. ' "src\\CUDARenderer.cu"'
         }
   end

   filter "system:windows"
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS" }

   filter "configurations:Debug"
      defines { "WL_DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "WL_RELEASE" }
      runtime "Release"
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      kind "WindowedApp"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"