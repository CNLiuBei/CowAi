@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ============================================
:: CowAI 源码备份脚本
:: 自动从 version.h 读取版本号
:: 排除编译产物、发布包、模型等大文件
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

:: 生成时间戳 YYYYMMDD_HHMMSS
for /f %%a in ('powershell -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TIMESTAMP=%%a"

set "BACKUP_NAME=CowAi_src_v%VER%_%TIMESTAMP%"
set "BACKUP_DIR=D:\sunone_backup\%BACKUP_NAME%"
set "ZIP=D:\sunone_backup\%BACKUP_NAME%.zip"

echo ============================================
echo   CowAI 源码备份工具
echo   版本: %VER%
echo   时间: %TIMESTAMP%
echo   输出: %ZIP%
echo ============================================

:: 创建备份目录
if not exist "D:\sunone_backup" mkdir "D:\sunone_backup"
if exist "%BACKUP_DIR%" rmdir /s /q "%BACKUP_DIR%"
mkdir "%BACKUP_DIR%"

:: 复制源码目录
echo [复制] 源码文件...
xcopy /s /e /y /q "sunone_aimbot_cpp\*" "%BACKUP_DIR%\sunone_aimbot_cpp\" >nul

:: 复制解决方案文件
echo [复制] 项目文件...
if exist "sunone_aimbot_cpp.sln" copy /y "sunone_aimbot_cpp.sln" "%BACKUP_DIR%\" >nul

:: 复制脚本
echo [复制] 脚本文件...
for %%f in (build_project.bat build_project_dml.bat pack_dml.bat backup_src.bat) do (
    if exist "%%f" copy /y "%%f" "%BACKUP_DIR%\" >nul
)

:: 复制配置
echo [复制] 配置文件...
if exist "config.ini" copy /y "config.ini" "%BACKUP_DIR%\" >nul
if exist ".gitignore" copy /y ".gitignore" "%BACKUP_DIR%\" >nul

:: 删除编译中间产物
echo [清理] 编译产物...
if exist "%BACKUP_DIR%\sunone_aimbot_cpp\x64" rmdir /s /q "%BACKUP_DIR%\sunone_aimbot_cpp\x64"
if exist "%BACKUP_DIR%\sunone_aimbot_cpp\Debug" rmdir /s /q "%BACKUP_DIR%\sunone_aimbot_cpp\Debug"
if exist "%BACKUP_DIR%\sunone_aimbot_cpp\Release" rmdir /s /q "%BACKUP_DIR%\sunone_aimbot_cpp\Release"
for /r "%BACKUP_DIR%" %%f in (*.obj *.pch *.pdb *.ilk *.idb *.tlog *.log *.ipch) do del /f "%%f" 2>nul

:: 压缩
echo [压缩] 生成 %ZIP%...
powershell -Command "Compress-Archive -Path '%BACKUP_DIR%' -DestinationPath '%ZIP%' -Force"

:: 清理临时目录
rmdir /s /q "%BACKUP_DIR%"

:: 统计
for %%f in ("%ZIP%") do (
    set /a "SIZE_MB=%%~zf / 1048576"
)
echo.
echo ============================================
echo   备份完成!
echo   文件: %ZIP%
echo   大小: !SIZE_MB! MB
echo ============================================
pause
