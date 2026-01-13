#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""备份源码脚本 - 只备份源代码文件，排除编译产物和大文件"""

import os
import zipfile
from datetime import datetime

# 配置
SOURCE_DIR = "sunone_aimbot_cpp"
EXCLUDE_DIRS = {"x64", "Debug", "Release", "CUDA", "DML", ".vs", "packages", "ipch", "eigen-3.4.0", "modules"}
EXCLUDE_EXTS = {".exe", ".dll", ".lib", ".obj", ".pdb", ".ilk", ".exp", ".idb", ".zip", ".rar", ".7z", ".pch", ".iobj", ".ipdb"}
ROOT_FILES = ["config.ini", "sunone_aimbot_cpp.sln", "README.md", ".gitignore"]

def should_exclude(path):
    """检查是否应该排除该文件"""
    parts = path.replace("\\", "/").split("/")
    for part in parts:
        if part in EXCLUDE_DIRS:
            return True
    ext = os.path.splitext(path)[1].lower()
    if ext in EXCLUDE_EXTS:
        return True
    return False

def main():
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    zip_name = f"sunone_src_{timestamp}.zip"
    
    file_count = 0
    with zipfile.ZipFile(zip_name, 'w', zipfile.ZIP_DEFLATED) as zf:
        # 备份源码目录
        for root, dirs, files in os.walk(SOURCE_DIR):
            # 过滤掉排除的目录
            dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
            
            for file in files:
                filepath = os.path.join(root, file)
                if not should_exclude(filepath):
                    zf.write(filepath)
                    file_count += 1
        
        # 备份根目录文件
        for f in ROOT_FILES:
            if os.path.exists(f):
                zf.write(f)
                file_count += 1
    
    size_mb = os.path.getsize(zip_name) / (1024 * 1024)
    print(f"备份完成: {zip_name}")
    print(f"文件数: {file_count}, 大小: {size_mb:.2f} MB")

if __name__ == "__main__":
    main()
