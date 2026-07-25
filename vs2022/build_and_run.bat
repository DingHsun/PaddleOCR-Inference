@echo off
:: =====================================================================
:: 建置 api_server 並直接執行
:: 用法：build_and_run.bat [Debug|Release]
:: 預設：build_and_run.bat Debug
:: =====================================================================
setlocal

set TARGET=api_server

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i

if not defined MSBUILD (
    echo [Error] 找不到 MSBuild.exe（vswhere 沒找到）。請改用「Developer Command Prompt for VS 2022」手動執行 msbuild，或確認 VS 安裝是否包含 C++ 建置工具。
    exit /b 1
)

echo ------ 建置 %TARGET% (%CONFIG%^|x64) ------
"%MSBUILD%" "%SCRIPT_DIR%PaddleOCR-cpp.slnx" /t:%TARGET% /p:Configuration=%CONFIG% /p:Platform=x64 /m /nologo
if errorlevel 1 (
    echo [Error] 建置失敗。
    exit /b 1
)

set EXE=%SCRIPT_DIR%x64\%CONFIG%\%TARGET%.exe
if not exist "%EXE%" (
    echo [Error] 建置成功但找不到執行檔：%EXE%
    exit /b 1
)

echo ------ 啟動 %TARGET%.exe (工作目錄=%REPO_ROOT%) ------
start "" /D "%REPO_ROOT%" "%EXE%"

endlocal
