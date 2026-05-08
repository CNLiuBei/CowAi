@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
echo === Building sunone_aimbot_cpp (DML) ===
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "D:\sunone_src\sunone_src\sunone_aimbot_cpp.sln" /p:Configuration=DML /p:Platform=x64 /m /v:minimal 2>&1
echo === Build exit code: %ERRORLEVEL% ===
