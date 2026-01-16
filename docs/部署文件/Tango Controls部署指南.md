这是一份基于我们共同排查经历，为您量身定制的 **Ubuntu 24.04 (WSL2) Tango Controls 完美部署指南**。

这份指南**避开了** Ubuntu 24.04 官方包自动配置的所有陷阱（数据库版本不兼容、端口冲突、Systemd 自动退出、Java 主机名解析错误），采用 **“手动铺路”** 的策略，确保一次成功。

---

# Tango Controls on Ubuntu 24.04 (WSL2) 部署指南

## 准备工作
1.  **环境**：Windows 10/11, WSL2。
2.  **系统**：纯净的 Ubuntu 24.04 实例（建议 `wsl --unregister` 后重装）。
3.  **目标**：
    *   数据库端口：**3307** (避开 Windows MySQL)。
    *   Tango Host：**127.0.0.1:10000** (强制 IPv4)。
    *   工具：Jive, Astor, Python3。

---

## 第一步：系统初始化 (换源与 Systemd)

进入 WSL 终端，执行以下操作：

```bash
# 1. 替换为清华源 (提升下载速度)
sudo sed -i 's@//.*archive.ubuntu.com@//mirrors.tuna.tsinghua.edu.cn@g' /etc/apt/sources.list.d/ubuntu.sources
sudo sed -i 's@//.*security.ubuntu.com@//mirrors.tuna.tsinghua.edu.cn@g' /etc/apt/sources.list.d/ubuntu.sources
sudo apt update

# 2. 开启 Systemd (Tango 必须)
# 如果文件里已经有 boot 配置，请手动修改，否则直接执行下面这行
echo -e "[boot]\nsystemd=true" | sudo tee /etc/wsl.conf
```

**⚠️ 关键操作：** 执行完上述命令后，必须在 Windows PowerShell 中重启 WSL：
```powershell
wsl --shutdown
```
然后重新打开 Ubuntu 终端。

---

## 第二步：安装与配置数据库 (MariaDB)

我们手动配置数据库，避开自动安装脚本的逻辑。

```bash
# 1. 安装 MariaDB
sudo apt install mariadb-server -y

# 2. 修改端口为 3307 (避免与 Windows 冲突)
sudo sed -i 's/port\s*=\s*3306/port = 3307/g' /etc/mysql/mariadb.conf.d/50-server.cnf
# 强制追加一行以防万一
echo "port = 3307" | sudo tee -a /etc/mysql/mariadb.conf.d/50-server.cnf

# 3. 重启数据库
sudo systemctl restart mariadb

# 4. 创建 Tango 专用数据库和用户
# 允许任意 IP 连接，方便调试
sudo mysql -e "CREATE DATABASE tango; \
CREATE USER 'tango'@'%' IDENTIFIED BY 'tango'; \
CREATE USER 'tango'@'localhost' IDENTIFIED BY 'tango'; \
CREATE USER 'tango'@'127.0.0.1' IDENTIFIED BY 'tango'; \
GRANT ALL PRIVILEGES ON tango.* TO 'tango'@'%'; \
GRANT ALL PRIVILEGES ON tango.* TO 'tango'@'localhost'; \
GRANT ALL PRIVILEGES ON tango.* TO 'tango'@'127.0.0.1'; \
FLUSH PRIVILEGES;"
```

---

## 第三步：安装 Tango 软件包 (避坑关键)

```bash
# 1. 安装核心包
sudo apt install tango-db tango-common tango-starter python3-tango libtango-dev -y
```

**🔴 高能预警：紫色弹窗选择**
*   当询问 **"Configure database for tango-db with dbconfig-common?"** 时：
*   **务必选择：< No > (否)**

---

## 第四步：初始化数据库结构 (修正版 SQL)

因为选了 No，我们需要手动导入表结构。这份 SQL **修复了 Ubuntu 24.04 缺少的 `id` 列**，并预置了 Starter 注册信息。

```bash
# 1. 生成配置文件
sudo tee /etc/tangorc > /dev/null <<EOF
[mysql]
host=127.0.0.1:3307
user=tango
password=tango
EOF
sudo chmod 644 /etc/tangorc

# 2. 获取当前主机名 (用于注册 Starter)
MY_HOST=$(hostname)
# 确保首字母大写 (如 GuandeBook)，如果你的 hostname 是全小写但你想要大写，请手动修改这里
# MY_HOST="GuandeBook" 

# 3. 导入数据库结构 (直接复制整段执行)
sudo mysql -u tango -p'tango' -P 3307 -h 127.0.0.1 tango <<EOF
-- 核心表
CREATE TABLE IF NOT EXISTS property_class (class varchar(255), attribute varchar(255), name varchar(255), count int(11) default 0, value text, updated timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, accessed timestamp NOT NULL default '2000-01-01 00:00:00', comment text, KEY index_class (class(64)), KEY index_attribute (attribute(64)), KEY index_name (name(64)));
CREATE TABLE IF NOT EXISTS property_device (device varchar(255), attribute varchar(255), name varchar(255), count int(11) default 0, value text, updated timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, accessed timestamp NOT NULL default '2000-01-01 00:00:00', comment text, KEY index_device (device(64)), KEY index_attribute (attribute(64)), KEY index_name (name(64)));
CREATE TABLE IF NOT EXISTS device (name varchar(255), alias varchar(255), domain varchar(255), family varchar(255), member varchar(255), class varchar(255), server varchar(255), pid int(11) default 0, exported int(11) default 0, ior text, host varchar(255), version varchar(255), started timestamp NOT NULL default '2000-01-01 00:00:00', stopped timestamp NOT NULL default '2000-01-01 00:00:00', KEY index_name (name(64)), KEY index_class (class(64)), KEY index_server (server(64)));
CREATE TABLE IF NOT EXISTS server (name varchar(255), host varchar(255), mode int(11) default 0, level int(11) default 0, KEY index_name (name(64)));
CREATE TABLE IF NOT EXISTS property (object varchar(255), name varchar(255), count int(11) default 0, value text, updated timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, accessed timestamp NOT NULL default '2000-01-01 00:00:00', comment text, KEY index_object (object(64)), KEY index_name (name(64)));

-- 历史表 (关键修正：加上 id 列)
CREATE TABLE IF NOT EXISTS property_class_hist (class varchar(255), attribute varchar(255), name varchar(255), count int(11) default 0, value text, updated timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, date timestamp NOT NULL default '2000-01-01 00:00:00', comment text, id int(11) default 0);
CREATE TABLE IF NOT EXISTS property_device_hist (device varchar(255), attribute varchar(255), name varchar(255), count int(11) default 0, value text, updated timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, date timestamp NOT NULL default '2000-01-01 00:00:00', comment text, id int(11) default 0);
CREATE TABLE IF NOT EXISTS property_attribute_class_hist (class varchar(255), attribute varchar(255), name varchar(255), count int(11) default 0, value text, updated timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, date timestamp NOT NULL default '2000-01-01 00:00:00', comment text, id int(11) default 0);
CREATE TABLE IF NOT EXISTS property_attribute_device_hist (device varchar(255), attribute varchar(255), name varchar(255), count int(11) default 0, value text, updated timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, date timestamp NOT NULL default '2000-01-01 00:00:00', comment text, id int(11) default 0);
CREATE TABLE IF NOT EXISTS property_hist (object varchar(255), name varchar(255), count int(11) default 0, value text, updated timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP, date timestamp NOT NULL default '2000-01-01 00:00:00', comment text, id int(11) default 0);

-- ID 计数器表
CREATE TABLE IF NOT EXISTS device_history_id (id int(11) default 0); INSERT INTO device_history_id VALUES (0);
CREATE TABLE IF NOT EXISTS property_history_id (id int(11) default 0); INSERT INTO property_history_id VALUES (0);

-- 注册 DataBaseds (自举)
INSERT IGNORE INTO device VALUES ('sys/database/2', 'sys', 'database', '2', 'DataBase', 'DataBaseds/2', 0, 0, '', '', '', '2000-01-01', '2000-01-01');
INSERT IGNORE INTO device VALUES ('dserver/DataBaseds/2', 'dserver', 'DataBaseds', '2', 'DServer', 'DataBaseds/2', 0, 0, '', '', '', '2000-01-01', '2000-01-01');
INSERT IGNORE INTO server VALUES ('DataBaseds/2', '127.0.0.1', 1, 0);

-- 注册 Starter (使用当前 hostname)
INSERT IGNORE INTO device VALUES ('tango/admin/${MY_HOST}', 'tango', 'admin', '${MY_HOST}', 'Starter', 'Starter/${MY_HOST}', 0, 0, '', '', '', '2000-01-01', '2000-01-01');
INSERT IGNORE INTO device VALUES ('dserver/Starter/${MY_HOST}', 'dserver', 'Starter', '${MY_HOST}', 'DServer', 'Starter/${MY_HOST}', 0, 0, '', '', '', '2000-01-01', '2000-01-01');

-- 解决 Java/Astor 在 WSL 中解析成 "127" 的 Bug (幽灵设备)
INSERT IGNORE INTO device VALUES ('tango/admin/127', 'tango', 'admin', '127', 'Starter', 'Starter/${MY_HOST}', 0, 0, '', '', '', '2000-01-01', '2000-01-01');
EOF
```

---

## 第五步：配置系统服务 (Systemd Overrides)

防止服务自动退出，并强制绑定 IPv4。

### 1. 配置 Tango-DB
```bash
sudo mkdir -p /etc/systemd/system/tango-db.service.d/
sudo tee /etc/systemd/system/tango-db.service.d/override.conf > /dev/null <<EOF
[Unit]
StopWhenUnneeded=false
[Service]
Type=simple
ExecStart=
# 注意文件名大小写，官方包通常是 Databaseds
ExecStart=/usr/lib/tango/Databaseds 2 -ORBendPoint giop:tcp:127.0.0.1:10000
Environment="MYSQL_HOST=127.0.0.1:3307"
Environment="MYSQL_USER=tango"
Environment="MYSQL_PASSWORD=tango"
EOF
```

### 2. 配置 Tango-Starter
```bash
# 获取主机名
MY_HOST=$(hostname)

sudo mkdir -p /etc/systemd/system/tango-starter.service.d/
sudo tee /etc/systemd/system/tango-starter.service.d/override.conf > /dev/null <<EOF
[Service]
Environment="TANGO_HOST=127.0.0.1:10000"
ExecStart=
# 强制指定主机名，防止 Systemd 和数据库里的大小写不一致
ExecStart=/usr/lib/tango/Starter ${MY_HOST}
EOF
```

---

## 第六步：启动与验证

```bash
# 1. 启动服务
sudo systemctl daemon-reload
sudo systemctl restart tango-db
sudo systemctl restart tango-starter
sudo systemctl enable tango-db tango-starter

# 2. 检查状态
sudo systemctl status tango-db
# 必须看到 active (running)

# 3. 设置环境变量
echo 'export TANGO_HOST=127.0.0.1:10000' >> ~/.bashrc
source ~/.bashrc

# 4. 验证核心功能
python3 -c "import tango; print('DB Status:', tango.DeviceProxy('sys/database/2').state())"
python3 -c "import tango, socket; print('Starter Status:', tango.DeviceProxy(f'tango/admin/{socket.gethostname()}').state())"
```
*如果你看到两个 `ON`，恭喜你，部署完成！*

---

## 第七步：安装图形工具 (Jive/Astor)

```bash
# 1. 安装 Java
sudo apt install default-jre -y
mkdir -p ~/tango-tools

# 2. 下载工具 (如果 WSL 下载慢，请在 Windows 下载后 cp 进去)
wget https://github.com/tango-controls/jive/releases/download/v7.3.0/Jive-7.3.0.jar -O ~/tango-tools/Jive.jar
wget https://repo1.maven.org/maven2/org/tango-controls/Astor/7.3.2/Astor-7.3.2.jar -O ~/tango-tools/Astor.jar

# 3. 配置别名 (解决 IPv4 解析问题)
echo "alias jive='java -jar ~/tango-tools/Jive.jar'" >> ~/.bashrc
# Astor 需要强制 IPv4 参数
echo "alias astor='java -Djava.net.preferIPv4Stack=true -jar ~/tango-tools/Astor.jar'" >> ~/.bashrc
source ~/.bashrc
```

**现在，你可以输入 `jive` 或 `astor` 启动图形界面了。**