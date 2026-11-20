@echo off
rem Optional launcher for machines with stale third-party Vulkan overlay manifests.
rem Only implicit layer discovery is isolated. Explicit Khronos validation stays enabled.
setlocal
pushd "%~dp0..\build-vs2022"
if errorlevel 1 exit /b 1
if not exist "empty-implicit-layers" mkdir "empty-implicit-layers"
set "VK_IMPLICIT_LAYER_PATH=%CD%\empty-implicit-layers"
Debug\cg-lab4.exe %*
set "lab4Exit=%errorlevel%"
popd
exit /b %lab4Exit%
