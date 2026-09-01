# 六自由度机器人调试 GUI

这是一个面向 Ubuntu 24.04 x86-64 的独立调试工具。它直接调用 `lib/libLTSMC.so` 访问运动控制器，并通过 TCP 读取编码器，不依赖 Tango。

## 功能

- 运动控制器与编码器连接状态；
- 六轴编码器位置显示；
- 相对/绝对位移与姿态控制；
- Stewart 平台逆运动学；
- 速度、加减速和等效脉冲配置；
- OUT3 刹车电源控制；
- 停止与急停。

## 环境

```bash
sudo apt install -y python3 python3-venv python3-pip

python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r gui/six_dof_debug_gui/requirements.txt

file lib/libLTSMC.so
ldd lib/libLTSMC.so
```

`ldd` 输出不能包含 `not found`。主机网卡需要能访问配置中的运动控制器和编码器 IP。

## 运行

从项目根目录启动：

```bash
python3 -m gui.six_dof_debug_gui.main
```

配置文件为 `gui/six_dof_debug_gui/config.json`。默认运动控制器地址是 `192.168.1.13`，编码器地址是 `192.168.1.199:5000`；现场使用前必须核对。

## 安全要求

- 首次连接先保持驱动器断电，确认设备身份和轴映射；
- 首次运动使用最低速度与小位移；
- 确认急停、限位和刹车功能正常；
- 禁止在机械行程未知、人员位于运动范围内或无人监护时发送运动命令；
- GUI 异常退出后，必须通过现场状态确认刹车与驱动器电源。

## 源码结构

```text
gui/six_dof_debug_gui/
├── main.py                 # 启动入口
├── main_window.py          # 主窗口
├── config.py               # 配置管理
├── hardware/               # 控制器、编码器和脉冲计算
├── kinematics/             # Stewart 运动学
└── widgets/                # GUI 组件
```
