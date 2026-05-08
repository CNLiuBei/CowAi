@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ============================================
:: CowAI DML 打包脚本
:: 自动从 version.h 读取版本号
:: 排除卡密信息 (key_code.txt)
:: ============================================

:: 从 version.h 提取版本号
for /f "tokens=3 delims= " %%a in ('findstr /C:"#define APP_VERSION " sunone_aimbot_cpp\version\version.h') do (
    set "VER=%%~a"
)
if "%VER%"=="" (
    echo [错误] 无法从 version.h 读取版本号
    pause
    exit /b 1
)

set "FOLDER=CowAi_v%VER%_DML"
set "ZIP=%FOLDER%.zip"
set "EXE_SRC=x64\DML\ai.exe"
set "DLL_DIR=CowAi_v2.0.4_DML"

echo ============================================
echo   CowAI DML 打包工具
echo   版本: %VER%
echo   输出: %FOLDER%
echo ============================================

:: 检查编译产物
if not exist "%EXE_SRC%" (
    echo [错误] 未找到 %EXE_SRC%，请先编译 DML 版本
    pause
    exit /b 1
)

:: 清理旧目录
if exist "%FOLDER%" (
    echo [清理] 删除旧目录 %FOLDER%
    rmdir /s /q "%FOLDER%"
)
if exist "%ZIP%" (
    del /f "%ZIP%"
)

:: 创建目录结构
echo [创建] 目录结构...
mkdir "%FOLDER%"
mkdir "%FOLDER%\models"
mkdir "%FOLDER%\peijian"
mkdir "%FOLDER%\recoil"
mkdir "%FOLDER%\screenshots"

:: 复制主程序
echo [复制] 主程序...
copy /y "%EXE_SRC%" "%FOLDER%\%FOLDER%.exe" >nul

:: 复制 DLL 依赖
echo [复制] DLL 依赖...
for %%f in (
    concrt140.dll
    msvcp140.dll
    msvcp140_1.dll
    vcruntime140.dll
    vcruntime140_1.dll
    DirectML.dll
    DirectML.Debug.dll
    glfw3.dll
    libcurl.dll
    onnxruntime.dll
    onnxruntime_providers_shared.dll
    opencv_world4130.dll
) do (
    if exist "%DLL_DIR%\%%f" (
        copy /y "%DLL_DIR%\%%f" "%FOLDER%\%%f" >nul
    ) else (
        echo [警告] 未找到 %DLL_DIR%\%%f
    )
)

:: 复制罗技驱动 DLL
if exist "ghub_mouse.dll" (
    echo [复制] ghub_mouse.dll
    copy /y "ghub_mouse.dll" "%FOLDER%\ghub_mouse.dll" >nul
) else (
    echo [警告] 未找到 ghub_mouse.dll
)

:: 复制资源文件（排除卡密）
echo [复制] 资源文件...
if exist "%DLL_DIR%\Cow.png" copy /y "%DLL_DIR%\Cow.png" "%FOLDER%\" >nul
if exist "%DLL_DIR%\config.ini" copy /y "%DLL_DIR%\config.ini" "%FOLDER%\" >nul

:: 复制模型
echo [复制] 模型文件...
if exist "models\*.onnx" (
    copy /y "models\*.onnx" "%FOLDER%\models\" >nul
)

:: 复制配件模板
echo [复制] 配件模板...
if exist "%DLL_DIR%\peijian" (
    xcopy /s /e /y /q "%DLL_DIR%\peijian\*" "%FOLDER%\peijian\" >nul
)

:: 复制后坐力数据
echo [复制] 后坐力数据...
if exist "%DLL_DIR%\recoil" (
    xcopy /s /e /y /q "%DLL_DIR%\recoil\*" "%FOLDER%\recoil\" >nul
)

:: 复制更新说明
if exist "%FOLDER%\更新说明.txt" (
    echo [已有] 更新说明
) else if exist "更新说明.txt" (
    copy /y "更新说明.txt" "%FOLDER%\" >nul
)

:: 安全检查：确保没有卡密文件
if exist "%FOLDER%\key_code.txt" (
    echo [安全] 删除卡密文件 key_code.txt
    del /f "%FOLDER%\key_code.txt"
)
if exist "%FOLDER%\auth_token.dat" (
    echo [安全] 删除旧版卡密文件 auth_token.dat
    del /f "%FOLDER%\auth_token.dat"
)

:: 压缩
echo [压缩] 生成 %ZIP%...
powershell -Command "Compress-Archive -Path '%FOLDER%' -DestinationPath '%ZIP%' -Force"

:: 统计
echo.
echo ============================================
echo   打包完成!
echo   目录: %FOLDER%
echo   压缩: %ZIP%
for %%f in ("%ZIP%") do (
    set /a "SIZE_MB=%%~zf / 1048576"
    echo   大小: !SIZE_MB! MB
)
echo.
echo   [安全] 已排除 key_code.txt
echo   [包含] ghub_mouse.dll
echo ============================================
pause
