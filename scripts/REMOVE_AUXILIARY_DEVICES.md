# 如何移除多余的辅助支撑设备

## 问题
服务器启动时创建了 10 个设备，但只需要 5 个：
- ✅ 保留：sys/auxiliary/1 到 sys/auxiliary/5
- ❌ 删除：ray_upper, ray_lower, reflection_upper, reflection_lower, targeting

## 解决方法

### 方法 1：使用 tango_admin 命令行工具（推荐）

1. **停止服务器**（如果正在运行）

2. **查看当前服务器定义**：
   ```bash
   tango_admin --server auxiliary_support_server/auxiliary --info
   ```

3. **更新服务器设备列表**：
   ```bash
   tango_admin --server auxiliary_support_server/auxiliary \
     --device-list \
     sys/auxiliary/1 \
     sys/auxiliary/2 \
     sys/auxiliary/3 \
     sys/auxiliary/4 \
     sys/auxiliary/5
   ```

4. **验证**：
   ```bash
   tango_admin --server auxiliary_support_server/auxiliary --info
   ```

5. **重启服务器**

### 方法 2：检查服务器配置文件

服务器配置可能在以下位置：
- `/etc/tango/servers/auxiliary_support_server.ini`
- `/etc/tango/servers/auxiliary.ini`

如果找到配置文件，编辑它，将设备列表改为：
```
sys/auxiliary/1 sys/auxiliary/2 sys/auxiliary/3 sys/auxiliary/4 sys/auxiliary/5
```

### 方法 3：如果设备是通过命令行启动的

检查 `scripts/start_servers.py`，如果启动命令包含设备列表，移除多余的设备。

### 验证

重启服务器后，查看日志，应该只看到 5 个设备被创建：
- sys/auxiliary/1
- sys/auxiliary/2  
- sys/auxiliary/3
- sys/auxiliary/4
- sys/auxiliary/5

