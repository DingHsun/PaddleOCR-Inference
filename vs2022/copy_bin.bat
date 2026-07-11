@echo off
:: =====================================================================
:: 現代化建置後自動搬檔腳本 (相容 ONNX Runtime 1.21.0 GPU 與 OpenCV)
:: 接收參數：%1 = $(TargetDir), %2 = $(PlatformName) , %3 = $(ConfigurationName) 
:: =====================================================================

set DestinationPath=%1
set PlatformName=%2
set ConfigurationName=%3

:: 移除可能被 VS 帶入的雙引號防呆
set DestinationPath=%DestinationPath:"=%
set PlatformName=%PlatformName:"=%

:: 🚀 直接將批次檔所在的資料夾（%~dp0）設定為 SolutionDir:: %~dp0 代表此批次檔所在目錄 (...\packages\)，再往上一層就是 SolutionDir
set SolutionDir=%~dp0

echo ------ 開始複製 ------
echo SolutionDir=%SolutionDir%
echo ConfigurationName=%ConfigurationName%
echo DestinationPath=%DestinationPath%
echo --------------------

:: 如果目標資料夾不存在則建立
if not exist "%DestinationPath%" mkdir "%DestinationPath%"

:: ─────────────────────────────────────────────────────────────────────
:: 1. 複製 OpenCV 核心組件
:: ─────────────────────────────────────────────────────────────────────
:: 複製不分組態的 ffmpeg 影片解碼庫
xcopy "%SolutionDir%..\packages\opencv\build\x64\vc15\bin\opencv_videoio_ffmpeg450_64.dll" "%DestinationPath%\" /d /y /q

if /I "%ConfigurationName%" == "Debug" (
    echo [Deploy] 複製 OpenCV Debug DLL...
    xcopy "%SolutionDir%..\packages\opencv\build\x64\vc15\bin\opencv_world450d.dll" "%DestinationPath%\" /d /y /q
)
if /I "%ConfigurationName%" == "Release" (
    echo [Deploy] 複製 OpenCV Release DLL...
    xcopy "%SolutionDir%..\packages\opencv\build\x64\vc15\bin\opencv_world450.dll" "%DestinationPath%\" /d /y /q
)

:: ─────────────────────────────────────────────────────────────────────
:: 2. 複製全新的 ONNX Runtime 1.21.0 GPU 核心與提供者 DLL
:: ─────────────────────────────────────────────────────────────────────
echo [Deploy] 複製 ONNX Runtime 1.21.0 GPU 資源...
set ONNX_LIB_DIR=%SolutionDir%..\packages\onnxruntime-win-x64-gpu-1.21.0\lib

xcopy "%ONNX_LIB_DIR%\onnxruntime.dll"                      "%DestinationPath%\" /d /y /q
xcopy "%ONNX_LIB_DIR%\onnxruntime_providers_cuda.dll"       "%DestinationPath%\" /d /y /q
xcopy "%ONNX_LIB_DIR%\onnxruntime_providers_shared.dll"     "%DestinationPath%\" /d /y /q
xcopy "%ONNX_LIB_DIR%\onnxruntime_providers_tensorrt.dll"   "%DestinationPath%\" /d /y /q

:: ─────────────────────────────────────────────────────────────────────
:: 3. 複製 weights 與 images（demo/api_server 共用）
:: ─────────────────────────────────────────────────────────────────────
xcopy "%SolutionDir%..\weights" "%DestinationPath%\weights" /E /I /Y
xcopy "%SolutionDir%..\images"  "%DestinationPath%\images"  /E /I /Y

echo ------ 結束複製 ------
exit /B 0