# CowAI

基于 TensorRT 的高性能 AI 视觉辅助工具。

## 功能特性

- TensorRT 加速推理，低延迟高性能
- 支持 YOLO 系列模型 (.onnx / .engine)
- ByteTrack 目标跟踪
- 多种输入方式支持 (Win32 / Ghub / Arduino / KMBox / Makcu)
- 可视化控制面板 (ImGui)
- 卡密授权系统

## 系统要求

- Windows 10/11 (x64)
- NVIDIA 显卡 (GTX 1660 及以上)
- CUDA 13.1
- TensorRT 10.x
- Visual C++ Redistributable 2022

## 快速开始

1. 将模型文件 (.engine 或 .onnx) 放入 `models` 目录
2. 运行 `ai.exe`
3. 首次运行需要输入卡密激活
4. 按 `Home` 键打开控制面板

## 按键说明

| 按键 | 功能 |
|------|------|
| 鼠标右键 | 瞄准 |
| Home | 打开/关闭控制面板 |
| F2 | 退出程序 |
| F3 | 暂停/恢复 |
| F4 | 重载配置 |

## 目录结构

```
├── ai.exe              # 主程序
├── config.ini          # 配置文件
├── models/             # 模型目录
│   ├── *.engine        # TensorRT 引擎文件
│   └── *.onnx          # ONNX 模型文件
└── auth_token.dat      # 授权信息
```

## 配置说明

配置文件 `config.ini` 包含以下主要设置：

- **AI 设置**: 模型选择、置信度阈值、检测分辨率
- **瞄准设置**: FOV、速度、预测参数
- **输入设置**: 输入方式、串口配置
- **界面设置**: 窗口显示、调试信息

详细配置请在控制面板中调整。

## 授权系统

本软件使用卡密授权系统：

1. 首次运行时输入卡密激活
2. 授权信息绑定设备
3. 支持时效卡和永久卡

## 编译说明

### 依赖项

- Visual Studio 2022
- CUDA Toolkit 13.1
- TensorRT 10.x
- OpenCV 4.13 (with CUDA)
- cuDNN 9.x

### 编译步骤

1. 打开 `sunone_aimbot_cpp.sln`
2. 选择 `Release | x64 | CUDA` 配置
3. 编译解决方案
4. 输出文件位于 `x64/CUDA/`

## 技术栈

- C++17
- TensorRT / CUDA
- OpenCV (CUDA)
- ImGui
- WinHTTP

## 免责声明

本软件仅供学习和研究使用，请勿用于任何违反游戏服务条款的行为。使用本软件造成的任何后果由用户自行承担。
