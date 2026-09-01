# Tango Controls on Ubuntu 24.04 部署指南

## 1. 环境约束

- 操作系统：Ubuntu 24.04 x86-64；
- 服务管理：systemd；
- 数据库：MariaDB；
- Tango 默认端口：`10000`；
- 项目构建目录：`build/`；
- 运动控制库：`lib/libLTSMC.so`。

生产环境应为 Tango Database、Starter 和各设备服务建立独立的 systemd unit，并使用固定主机名或静态地址。

## 2. 基础软件

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git \
  mariadb-server libomniorb4-dev libzmq3-dev qtbase5-dev \
  python3 python3-venv python3-pip
```

Tango Controls、open62541、Snap7 与厂商 SDK 按现场锁定版本安装。安装完成后记录包版本，避免设备机之间漂移。

## 3. 数据库与 Tango

```bash
sudo systemctl enable --now mariadb
sudo systemctl status mariadb

export TANGO_HOST=127.0.0.1:10000
tango_admin --ping-database
```

如果数据库部署在独立主机，把 `TANGO_HOST` 设置为该主机的 `host:port`，并将它写入设备服务的 systemd `Environment=` 配置。

## 4. 项目部署

```bash
git clone https://github.com/luxiaoze-up/doubleconeshooting.git
cd doubleconeshooting

python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python -m pip install -r gui/requirements.txt
python -m pip install -r gui/vacuum_system_gui/requirements.txt

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

验证厂商库：

```bash
file lib/libLTSMC.so
ldd lib/libLTSMC.so
```

## 5. 设备注册与启动顺序

```bash
python3 scripts/register_devices.py --config config/devices_config.json --force
python3 scripts/start_servers.py
scripts/start_vacuum_system.sh
```

推荐顺序：MariaDB → Tango Database → Starter → 运动控制器/编码器 → 设备服务 → 真空系统 → GUI。

## 6. 上线检查

- `tango_admin --ping-database` 成功；
- 每个服务的实例名与数据库注册一致；
- 控制器 IP、轴映射、限位和 IO 与现场表一致；
- `ldd lib/libLTSMC.so` 没有 `not found`；
- 日志目录可写且已配置轮转；
- 急停、刹车和断电保护已做现场验证；
- 服务重启后能恢复到安全状态。

不要在无人监护时运行带运动或电源切换的诊断脚本。
