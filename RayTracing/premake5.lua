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

      -- CUDA architecture flags string
      local cudaArchFlags = getCudaArchFlags()

      -- Shared NVCC command arguments (excluding config-specific flags)
      local function nvccCommonArgs()
         return ' -rdc=true'
            .. ' -lineinfo'
            .. ' --use_fast_math'
            .. ' -I"' .. cudaPath .. '/include"'
            .. ' -I"../Walnut/vendor/imgui"'
            .. ' -I"../Walnut/vendor/glfw/include"'
            .. ' -I"../Walnut/vendor/glm"'
            .. ' -I"../Walnut/Walnut/src"'
            .. ' -I"%{IncludeDir.VulkanSDK}"'
            .. ' -DWL_PLATFORM_WINDOWS'
            .. ' -DWL_CUDA'
            .. ' ' .. cudaArchFlags
      end

      -- Custom build rule for .cu files using NVCC
      filter "files:**.cu"

         -- Ensure output directory exists before NVCC runs
         prebuildcommands {
            '{MKDIR} "%{cfg.objdir}"'
         }

         -- Debug
         filter "configurations:Debug"
            buildmessage "Compiling %{file.relpath} with NVCC (Debug)"
            buildcommands {
               '"' .. cudaPath .. '/bin/nvcc"'
               .. ' -ccbin="$(VCToolsInstallDir)bin\\Hostx64\\x64"'
               .. ' -Xcompiler "/EHsc,/W3,/nologo,/Zi,/MDd"'
               .. ' -G'
               .. ' -DWL_DEBUG'
               .. nvccCommonArgs()
               .. ' --compile'
               .. ' -o "%{cfg.objdir}\\%{file.basename}.obj"'
               .. ' "%{file.relpath}"'
            }
            buildoutputs { "%{cfg.objdir}\\%{file.basename}.obj" }

         -- Release
         filter "configurations:Release"
            buildmessage "Compiling %{file.relpath} with NVCC (Release)"
            buildcommands {
               '"' .. cudaPath .. '/bin/nvcc"'
               .. ' -ccbin="$(VCToolsInstallDir)bin\\Hostx64\\x64"'
               .. ' -Xcompiler "/EHsc,/W3,/nologo,/Zi,/MD"'
               .. ' -O2'
               .. ' -DWL_RELEASE'
               .. nvccCommonArgs()
               .. ' --compile'
               .. ' -o "%{cfg.objdir}\\%{file.basename}.obj"'
               .. ' "%{file.relpath}"'
            }
            buildoutputs { "%{cfg.objdir}\\%{file.basename}.obj" }

         -- Dist
         filter "configurations:Dist"
            buildmessage "Compiling %{file.relpath} with NVCC (Dist)"
            buildcommands {
               '"' .. cudaPath .. '/bin/nvcc"'
               .. ' -ccbin="$(VCToolsInstallDir)bin\\Hostx64\\x64"'
               .. ' -Xcompiler "/EHsc,/W3,/nologo,/MD"'
               .. ' -O2'
               .. ' -DWL_DIST'
               .. nvccCommonArgs()
               .. ' --compile'
               .. ' -o "%{cfg.objdir}\\%{file.basename}.obj"'
               .. ' "%{file.relpath}"'
            }
            buildoutputs { "%{cfg.objdir}\\%{file.basename}.obj" }

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