# smc_pvt_move 调用前必须设置项与调用顺序

> 适用范围：本工程使用的 LTSMC/SMC600 系列接口；以 `smc_pvt_table_unit` + `smc_pvt_move`（PVT）为核心。
> 
> 备注：本仓库代码中未封装“伺服使能/清报警/复位”等接口；如现场出现 ALM/EMG 等问题，需要从厂商 `LTSMC.h` 查找并补充对应 `smc_*` 接口。

---

## 一、最小可用调用顺序（按先后）

### 0) 控制器连接（强制）
1. `smc_board_init(...)`
   - 本工程封装：`SMC606::SMC606_InitBoard()`

### 1) 轴必须处于“允许运动”的硬件状态（条件强制，但实操上等同必做）
在启动 PVT 前，务必确认：未急停、未报警、未触发限位、轴不处于其他运动中。

建议按每个参与轴执行：
1. 运动忙闲检查：`smc_check_done(connect, axis)`
   - 忙（正在运动）时，避免直接下发并启动新的 PVT。
2. 关键 IO 状态检查：`smc_axis_io_status(connect, axis)`
   - 关注位：`ALM`（报警）、`EMG`（急停）、`EL+ / EL-`（正/负硬限位）、`SL+ / SL-`（软限位）、`ORG`（原点）等。
3. 如需要停止当前运动：`smc_stop(connect, axis, stop_mode)`
4. 如需要整卡急停：`smc_emg_stop(connect)`

> 若 `ALM`/`EMG`/限位有效，通常需要先“清报警/解除急停/伺服使能”，否则 PVT 表下发成功也不会动。

### 2) 坐标/单位基准一致（条件强制：当你的 PVT 表用“绝对位置”或你必须对齐参考系时）
`smc_pvt_table_unit` 的 `pPos/pVel` 是“unit”版本。你需要明确：表里的位置是相对哪个零点/原点。

按每个参与轴，选择一种或组合：
1. 设定当前位置（规划坐标零点/当前位置对齐）：`smc_set_position_unit(connect, axis, pos)`
2. 设置工件原点：`smc_set_workpos_unit(connect, axis, pos)`
3. 如使用编码器闭环且需要对齐：`smc_set_encoder_unit(connect, axis, encoder_value)`

并建议校验读取：
- `smc_get_position_unit(connect, axis, &pos)`
- `smc_get_encoder_unit(connect, axis, &encoder)`

> 选择规则：
> - 若 `pPos[]` 为“绝对位置”：必须先把零点/工件原点设置到你期望的参考系。
> - 若 `pPos[]` 为“相对当前的增量”：至少要保证当前的位置读数/设定是正确的。

### 3) 下发 PVT 表（强制，必须先于启动）
对每个参与轴调用：
1. `smc_pvt_table_unit(connect, axis, count, pTime, pPos, pVel)`

**输入约束（必须满足，否则常见失败/异常）：**
- `count > 0`
- `pTime[]` 单调递增（通常从 0 开始或固定周期累加）
- 多轴同步时：各轴建议使用同一套时间基准（`count` 与 `pTime[]` 对齐）
- `pPos[] / pVel[]` 单位与控制器 unit 设置一致，且不超轴能力

### 4) 启动 PVT（强制，最后一步）
1. 准备轴列表：`AxisNum`、`AxisList[]`
2. 启动：`smc_pvt_move(connect, AxisNum, AxisList)`

### 5) 运行中/运行后监控（建议）
- 轮询完成：`smc_check_done(connect, axis)`
- 若出现异常：再次读取 `smc_axis_io_status` 判断 ALM/EMG/限位等。

---

## 二、推荐的“最小正确骨架”（伪代码）

```cpp
// 1) init
smc_board_init(connect, ...);

// 2) pre-check per axis
for axis in AxisList:
  smc_axis_io_status(connect, axis);
  if (!smc_check_done(connect, axis)) {
    // optional: smc_stop(connect, axis, stop_mode);
  }
  // if ALM/EMG/EL active -> clear/enable (vendor APIs, not in this repo)

// 3) coordinate alignment per axis (choose what you need)
for axis in AxisList:
  smc_set_position_unit(connect, axis, pos);
  // or smc_set_workpos_unit / smc_set_encoder_unit

// 4) table download per axis
for axis in AxisList:
  smc_pvt_table_unit(connect, axis, count, pTime, pPos[axis], pVel[axis]);

// 5) start
smc_pvt_move(connect, AxisNum, AxisList);

// 6) monitor
for axis in AxisList:
  while (smc_check_done(connect, axis) == 0) {
    // wait
  }
```

---

## 三、与本工程封装的对应关系（可选快速对照）

- `smc_board_init` → `SMC606::SMC606_InitBoard()`
- `smc_check_done` → `SMC606::SMC606_check_done(...)`
- `smc_axis_io_status` → `SMC606::SMC606_axis_io_status(...)`
- `smc_set_position_unit` → `SMC606::SMC606_set_position_unit(...)`
- `smc_set_workpos_unit` → `SMC606::SMC606_set_workpos_unit(...)`
- `smc_set_encoder_unit` → `SMC606::SMC606_set_encoder_unit(...)`
- `smc_pvt_table_unit` → `SMC606::SMC606_pvt_table_unit(...)`
- `smc_pvt_move` → `SMC606::SMC606_pvt_move(...)`
- `smc_stop` → `SMC606::SMC606_stop(...)`
- `smc_emg_stop` → `SMC606::SMC606_emg_stop(...)`
