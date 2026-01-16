# 六自由度机器人调试GUI

## 简介

这是一个专门用于测试六自由度机器人的独立GUI应用程序。GUI直接通过网络访问运动控制器和编码器采集器，不依赖Tango框架。

## 功能特性

### 1. 硬件连接配置
- 支持配置运动控制器的IP地址和端口号
- 支持配置编码器数据采集器的IP地址和端口号
- 实时显示连接状态
- 支持抱闸的打开/关闭控制（OUT3端口）

### 2. 位置控制
- X/Y/Z轴位置设定（单位：mm）
- 支持相对位置和绝对位置两种模式（全局模式）
- 实时执行运动命令

### 3. 旋转角度控制
- ThetaX/ThetaY/ThetaZ三个独立角度设定
- 支持度和弧度两种单位
- 支持相对旋转角和绝对旋转角两种模式（全局模式）

### 4. 编码器数据显示
- 实时显示6个轴的编码器原始值
- 实时显示转换后的位置值（mm）
- 更新频率可配置（默认100ms）

### 5. 速度参数配置
- 启动速度（StartSpeed）
- 最大速度（MaxSpeed）
- 加速度时间（AccTime）
- 减速度时间（DecTime）
- 停止速度（StopSpeed）
- 支持为单个轴或全部轴配置

### 6. 核心计算功能
- **脉冲数计算**：根据机械参数自动计算
  - 导程：2mm
  - 细分数：4000
  - 齿轮比：29/54
  - 减速比：1/8
- **运动学计算**：Stewart平台逆运动学，将X/Y/Z和旋转角度转换为6个轴的腿长变化

## 安装依赖

**注意：LTSMC.dll是32位版本，需要使用32位Python运行。**

```bash
# 使用32位Python安装依赖
py -3.14-32 -m pip install -r requirements.txt
# 或
py -3.11-32 -m pip install -r requirements.txt
```

## 运行

### 方法1：使用批处理脚本（推荐）

```bash
gui\six_dof_debug_gui\run_with_32bit.bat
```

### 方法2：直接运行

```bash
# 从项目根目录运行
py -3.14-32 -m gui.six_dof_debug_gui.main
# 或
py -3.11-32 -m gui.six_dof_debug_gui.main
```

### 方法3：从GUI目录运行

```bash
cd gui/six_dof_debug_gui
py -3.14-32 main.py
```

## 配置

配置文件位于 `gui/six_dof_debug_gui/config.json`，首次运行会自动创建默认配置。

### 默认配置

- **运动控制器**：192.168.1.13，卡ID=0
- **编码器采集器**：192.168.1.199:5000，通道0-5
- **脉冲计算参数**：导程2mm，细分数4000，齿轮比29/54，减速比1/8
- **运动学参数**：从config.json读取（r1, r2, hh, a1, a2, h, h3, ll）

## 使用说明

1. **连接设备**
   - 打开"连接配置"标签页
   - 输入运动控制器和编码器采集器的IP地址
   - 点击"连接"按钮

2. **控制抱闸**
   - 在"连接配置"标签页中
   - 勾选"释放抱闸"以释放抱闸
   - 取消勾选以启用抱闸

3. **位置控制**
   - 打开"位置控制"标签页
   - 选择相对或绝对位置模式
   - 输入X/Y/Z位置值（mm）
   - 点击"执行"按钮

4. **旋转控制**
   - 打开"旋转控制"标签页
   - 选择角度单位（度或弧度）
   - 选择相对或绝对旋转角模式
   - 输入ThetaX/ThetaY/ThetaZ角度值
   - 点击"执行"按钮

5. **查看编码器数据**
   - 打开"编码器数据"标签页
   - 实时查看6个轴的编码器读数

6. **配置速度参数**
   - 打开"速度配置"标签页
   - 选择要配置的轴（单个或全部）
   - 输入速度参数
   - 点击"应用"按钮

## 注意事项

1. **硬件要求**
   - 需要LTSMC.dll（Windows）或libLTSMC.so（Linux）库文件
   - 库文件应位于项目根目录的 `lib/` 文件夹中
   - **重要**: LTSMC.dll是32位版本，必须使用32位Python运行

2. **Python版本要求**
   - 必须使用32位Python（推荐Python 3.11或3.14）
   - 如果未安装32位Python，可以使用以下命令安装：
     ```powershell
     winget install Python.Python.3.11 --architecture x86
     ```

3. **网络要求**
   - 确保计算机能够访问运动控制器和编码器采集器的IP地址
   - 默认端口：运动控制器使用Modbus TCP（端口502），编码器采集器使用5000

4. **安全提示**
   - 运动前确保抱闸已释放
   - 运动后建议启用抱闸以保护设备
   - 注意运动范围限制，避免超出机械限位

## 技术架构

- **GUI框架**：PyQt5
- **硬件通信**：
  - 运动控制器：ctypes调用LTSMC.dll
  - 编码器采集器：TCP Socket通信
- **数学计算**：numpy（运动学计算）
- **配置管理**：JSON文件

## 文件结构

```
gui/six_dof_debug_gui/
├── main.py                 # 主入口
├── main_window.py          # 主窗口
├── config.py               # 配置管理
├── requirements.txt        # Python依赖
├── config.json            # 配置文件（自动生成）
├── run_with_32bit.bat      # 启动脚本（32位Python）
├── hardware/              # 硬件通信模块
│   ├── smc_controller.py  # 运动控制器通信
│   ├── encoder_collector.py  # 编码器采集器通信
│   └── pulse_calculator.py   # 脉冲数计算
├── kinematics/            # 运动学计算模块
│   └── stewart_kinematics.py  # Stewart平台逆运动学
└── widgets/               # UI控件
    ├── connection_widget.py      # 连接配置
    ├── position_control_widget.py # 位置控制
    ├── rotation_control_widget.py # 旋转控制
    ├── encoder_display_widget.py  # 编码器显示
    └── speed_config_widget.py     # 速度配置
```

## 打包为可执行程序

### 快速打包

使用批处理脚本（推荐）：
```bash
gui\six_dof_debug_gui\build_exe.bat
```

或使用Python脚本：
```bash
cd gui/six_dof_debug_gui
py -3.14-32 build_exe.py          # 文件夹模式
py -3.14-32 build_exe.py --onefile # 单文件模式
```

### 打包要求

- 使用32位Python（推荐Python 3.14或3.11）
- 已安装PyInstaller：`py -3.14-32 -m pip install pyinstaller`
- 已安装依赖库：`py -3.14-32 -m pip install PyQt5 numpy`

### 打包输出

- **文件夹模式**：`dist/SixDofDebugGUI/` 目录，包含exe和所有依赖
- **单文件模式**：`dist/SixDofDebugGUI.exe` 单个文件

### 部署到其他主机

详细部署要求请查看：`部署要求.md`

**基本步骤：**
1. 将打包后的 `dist/SixDofDebugGUI` 文件夹复制到目标主机
2. 确保 `lib/LTSMC.dll` 文件存在（会自动复制）
3. 双击运行 `SixDofDebugGUI.exe`

**目标主机要求：**
- Windows 7 或更高版本
- 无需安装Python
- 需要网络连接到运动控制器和编码器采集器

## 开发说明

本GUI基于现有六自由度机器人控制算法开发，主要参考了以下文件：
- `src/device_services/six_dof_device.cpp` - 六自由度设备控制逻辑
- `src/common/kinematics.cpp` - 运动学算法
- `src/device_services/motion_controller_device.cpp` - 运动控制器接口
- `scripts/test_io_output.py` - SMC库封装示例
- `scripts/test_encoder_collector_read.py` - 编码器通信示例
