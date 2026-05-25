-- CUDA Toolkit Detection
local cudaPath = os.getenv("CUDA_PATH") or os.getenv("CUDA_PATH_V12_8") or os.getenv("CUDA_PATH_V12_6") or os.getenv("CUDA_PATH_V12_5") or os.getenv("CUDA_PATH_V12_4") or os.getenv("CUDA_PATH_V11_8") or "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8"
local cudaFound = os.isdir(cudaPath)

-- CUDA Architecture Targets (virtual + real)
-- sm_75: Turing (RTX 20xx, GTX 16xx)
-- sm_86: Ampere (RTX 30xx)
-- sm_89: Ada Lovelace (RTX 40xx)
local cudaArchs = {
   "compute_75,sm_75",
   "compute_86,sm_86",
   "compute_89,sm_89",
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
      files { "src/**.cu", "src/**.cuh" }
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

      -- Custom build rule for .cu files using NVCC
      filter "files:**.cu"
         compileas "Cuda"
         buildmessage "Compiling %{file.relpath} with NVCC"
         buildcommands {
            '"' .. cudaPath .. '/bin/nvcc"'
            .. ' -ccbin="$(VC_ExecutablePath_x64_x64)"'
            .. ' -Xcompiler="/EHsc /W3 /nologo /FS /Zi /MD"'
            .. ' -I"' .. cudaPath .. '/include"'
            .. ' -I"../Walnut/vendor/imgui"'
            .. ' -I"../Walnut/vendor/glfw/include"'
            .. ' -I"../Walnut/vendor/glm"'
            .. ' -I"../Walnut/Walnut/src"'
            .. ' -I"%{IncludeDir.VulkanSDK}"'
            .. ' -DWL_PLATFORM_WINDOWS'
            .. ' -DWL_CUDA'
            .. getCudaArchFlags()
            .. ' -lineinfo'
            .. ' --use_fast_math'
            .. ' --compile'
            .. ' -o "%{cfg.objdir}/%{file.basename}.obj"'
            .. ' "%{file.relpath}"'
         }
         buildoutputs { "%{cfg.objdir}/%{file.basename}.obj" }
      filter {}

      filter { "files:**.cu", "configurations:Debug" }
         buildmessage "Compiling %{file.relpath} with NVCC (Debug)"
         buildcommands {
            '"' .. cudaPath .. '/bin/nvcc"'
            .. ' -ccbin="$(VC_ExecutablePath_x64_x64)"'
            .. ' -Xcompiler="/EHsc /W3 /nologo /FS /Zi /MDd"'
            .. ' -I"' .. cudaPath .. '/include"'
            .. ' -I"../Walnut/vendor/imgui"'
            .. ' -I"../Walnut/vendor/glfw/include"'
            .. ' -I"../Walnut/vendor/glm"'
            .. ' -I"../Walnut/Walnut/src"'
            .. ' -I"%{IncludeDir.VulkanSDK}"'
            .. ' -DWL_PLATFORM_WINDOWS'
            .. ' -DWL_CUDA'
            .. ' -DWL_DEBUG'
            .. getCudaArchFlags()
            .. ' -lineinfo'
            .. ' -G'
            .. ' --compile'
            .. ' -o "%{cfg.objdir}/%{file.basename}.obj"'
            .. ' "%{file.relpath}"'
         }
         buildoutputs { "%{cfg.objdir}/%{file.basename}.obj" }
      filter {}

      filter { "files:**.cu", "configurations:Release" }
         buildmessage "Compiling %{file.relpath} with NVCC (Release)"
         buildcommands {
            '"' .. cudaPath .. '/bin/nvcc"'
            .. ' -ccbin="$(VC_ExecutablePath_x64_x64)"'
            .. ' -Xcompiler="/EHsc /W3 /nologo /FS /Zi /MD"'
            .. ' -I"' .. cudaPath .. '/include"'
            .. ' -I"../Walnut/vendor/imgui"'
            .. ' -I"../Walnut/vendor/glfw/include"'
            .. ' -I"../Walnut/vendor/glm"'
            .. ' -I"../Walnut/Walnut/src"'
            .. ' -I"%{IncludeDir.VulkanSDK}"'
            .. ' -DWL_PLATFORM_WINDOWS'
            .. ' -DWL_CUDA'
            .. ' -DWL_RELEASE'
            .. getCudaArchFlags()
            .. ' -lineinfo'
            .. ' --use_fast_math'
            .. ' -O2'
            .. ' --compile'
            .. ' -o "%{cfg.objdir}/%{file.basename}.obj"'
            .. ' "%{file.relpath}"'
         }
         buildoutputs { "%{cfg.objdir}/%{file.basename}.obj" }
      filter {}

      filter { "files:**.cu", "configurations:Dist" }
         buildmessage "Compiling %{file.relpath} with NVCC (Dist)"
         buildcommands {
            '"' .. cudaPath .. '/bin/nvcc"'
            .. ' -ccbin="$(VC_ExecutablePath_x64_x64)"'
            .. ' -Xcompiler="/EHsc /W3 /nologo /FS /MD"'
            .. ' -I"' .. cudaPath .. '/include"'
            .. ' -I"../Walnut/vendor/imgui"'
            .. ' -I"../Walnut/vendor/glfw/include"'
            .. ' -I"../Walnut/vendor/glm"'
            .. ' -I"../Walnut/Walnut/src"'
            .. ' -I"%{IncludeDir.VulkanSDK}"'
            .. ' -DWL_PLATFORM_WINDOWS'
            .. ' -DWL_CUDA'
            .. ' -DWL_DIST'
            .. getCudaArchFlags()
            .. ' --use_fast_math'
            .. ' -O2'
            .. ' --compile'
            .. ' -o "%{cfg.objdir}/%{file.basename}.obj"'
            .. ' "%{file.relpath}"'
         }
         buildoutputs { "%{cfg.objdir}/%{file.basename}.obj" }
      filter {}
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