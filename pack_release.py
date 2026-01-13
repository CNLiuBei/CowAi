#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""打包发布版本"""

import os
import shutil
import zipfile
from datetime import datetime

# 配置
EXE_PATH = "x64/CUDA/ai.exe"
EXE_NAME = "CowAI.exe"
DLL_DIR = "x64/CUDA"
EXTRA_FILES = [
    "sunone_aimbot_cpp/ghub_mouse.dll",
]

# 必需的DLL列表（只包含实际需要的）
REQUIRED_DLLS = [
    "cudnn64_9.dll",
    "cudnn_cnn64_9.dll",
    "opencv_world4130.dll",
    "onnxruntime.dll",
    "onnxruntime_providers_shared.dll",
]

# 加密模型 DLL 文件（放在根目录）
ENCRYPTED_MODEL_DLLS = [
    'cudnn_cnn_train64_9.dll',   # PUBG8 模型
    'cudnn_adv_train64_9.dll',   # 默认模型（如果有）
]

BUY_URL = "https://cowai.trueliu.com/buy"

def main():
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    release_dir = f"CowAI_Release_{timestamp}"
    zip_name = f"CowAI_{timestamp}.zip"
    
    os.makedirs(release_dir, exist_ok=True)
    
    # 复制exe
    shutil.copy(EXE_PATH, os.path.join(release_dir, EXE_NAME))
    print(f"+ {EXE_NAME}")
    
    # 复制dll
    for dll in REQUIRED_DLLS:
        src = os.path.join(DLL_DIR, dll)
        if os.path.exists(src):
            shutil.copy(src, release_dir)
            print(f"+ {dll}")
        # 不存在的DLL不再提示警告，因为有些是可选的
    
    # 复制额外文件
    for f in EXTRA_FILES:
        if os.path.exists(f):
            shutil.copy(f, release_dir)
            print(f"+ {os.path.basename(f)}")
    
    # 复制加密模型 DLL 到根目录
    found_models = []
    for dll_name in ENCRYPTED_MODEL_DLLS:
        # DLL 文件现在在根目录
        if os.path.exists(dll_name):
            shutil.copy(dll_name, os.path.join(release_dir, dll_name))
            found_models.append(dll_name)
    
    if found_models:
        print(f"+ 加密模型 ({len(found_models)} 个)")
        for dll in found_models:
            print(f"    - {dll}")
    else:
        print(f"! 警告: 没有找到加密模型文件")
        print(f"! 请先运行 'python encrypt_model.py' 加密模型")
    
    # 创建空的 models 和 screenshots 目录
    os.makedirs(os.path.join(release_dir, "models"), exist_ok=True)
    os.makedirs(os.path.join(release_dir, "screenshots"), exist_ok=True)
    print("+ models/ (空目录)")
    print("+ screenshots/ (空目录)")
    
    # 创建购买链接
    url_file = os.path.join(release_dir, "购买授权.url")
    with open(url_file, "w", encoding="utf-8") as f:
        f.write(f"[InternetShortcut]\nURL={BUY_URL}\n")
    print("+ 购买授权.url")
    
    # 打包 (使用STORED模式加快速度，文件本身已经是压缩过的)
    with zipfile.ZipFile(zip_name, 'w', zipfile.ZIP_STORED) as zf:
        for root, _, files in os.walk(release_dir):
            for file in files:
                filepath = os.path.join(root, file)
                arcname = os.path.relpath(filepath, release_dir)
                zf.write(filepath, arcname)
                print(f"  压缩: {arcname}")
    
    # 清理
    shutil.rmtree(release_dir)
    
    size_mb = os.path.getsize(zip_name) / (1024 * 1024)
    print(f"\n发布包: {zip_name} ({size_mb:.1f} MB)")

if __name__ == "__main__":
    main()
