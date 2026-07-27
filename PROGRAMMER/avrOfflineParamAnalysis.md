# AVR 离线烧录参数分析

## 1. 分析范围

本次检查的对象有 4 部分：

- `PROGRAMMER/avrDeviceConst.h`
- `PROGRAMMER/avrDeviceConst.c`
- `USER/Stk500Protocol.c`
- `PROGRAMMER/isp.c`
- `PROGRAMMER/hvproc.c`
- `PROGRAMMER/avrOffLinePgm.c`
- `PROGRAMMER/avrdude-avr-init.xml`

判断标准采用“在线编程调用函数时，哪些参数真正参与了实际编程动作”，而不是简单比较结构体字段多少。

## 2. 在线编程实际会用到的 AVR 参数

### 2.1 ISP 路径

`isp.c` 中实际参与烧录动作的参数如下：

- `stabDelay`
  - `ispEnterProgmode()` 中用于 `ispAttachToDevice()` 的上电稳定等待
- `cmdExeDelay`
  - `ispEnterProgmode()` 中进入编程后额外等待
- `pollIndex`
  - `ispEnterProgmode()` 中决定 `ispBlockTransfer()` 首次返回比较位置
- `pollValue`
  - `ispEnterProgmode()` 中判断是否同步成功
- `preDelay`
  - `ispLeaveProgmode()` 中释放 RESET 前等待
- `postDelay`
  - `ispLeaveProgmode()` 中退出编程模式后等待
- `eraseDelay`
  - `ispChipErase()` 中超时或固定延时
- `pollMethod`
  - `ispChipErase()` 中决定“轮询等待”还是“固定延时”
- `mode`
  - `ispProgramMemory()` 中决定页写/字写、最后一页、轮询方式
- `delay`
  - `ispProgramMemory()` 中写后等待
- `poll[2]`
  - `ispProgramMemory()` 中值轮询比较字节
- `cmd[3]`
  - `ispProgramMemory()` 中写/写页/读回命令
- `LOAD_EXT_ADDR`
  - 大于 64KB Flash 时需要扩展地址命令

### 2.2 HVSP / PP 路径

`hvproc.c` 中实际参与烧录动作的参数如下：

- `stabDelay`
- `cmdExeDelay`
- `synchCycles`
- `latchCycles`
- `toggleVtg`
- `powerOffDelay`
- `resetDelay1/resetDelay2`
- `resetDelay`
- `progModeDelay`
- `pollTimeout`
- `eraseTime / pulseWidth`
- `mode`
- `delay`
- `fuseAddress / address`

其中一部分来自上位机命令实时下发，另一部分在完整离线重放时需要由器件常量表提供默认值或重建值。

## 3. 原 avrDeviceConst 模块已满足的参数

原有 `avrDeviceConst` 已经覆盖了以下参数：

- `signature`
- `stk500_devcode`
- `avr910_devcode`
- `timeout`
- `stabdelay`
- `cmdexedelay`
- `synchloops`
- `bytedelay`
- `pollvalue`
- `pollindex`
- `chip_erase_delay`
- `flash_page_size`
- `flash_size`
- `eeprom_size`
- `fuse_count`
- `op[11]`
- `mem[].size`
- `mem[].page_size`
- `mem[].readsize`
- `mem[].delay`

这些参数已经足够用于：

- 设备识别
- 进入 ISP 编程模式
- 基础擦除命令
- 生成基础离线参数包

## 4. 原 avrDeviceConst 模块不满足的参数

对照在线函数后，发现下面这些属于“在线实际会用到，但原常量表缺失”的参数：

### 4.1 ISP 离线重放缺失

- `predelay`
- `postdelay`
- `pollmethod`
- `mem[].mode`
- `mem[].readback[2]`

说明：

- `predelay/postdelay` 影响 `LEAVE_PROGMODE_ISP`
- `pollmethod` 影响 `CHIP_ERASE_ISP`
- `mode/readback` 影响 `PROGRAM_FLASH_ISP / PROGRAM_EEPROM_ISP`

这几项如果缺失，就只能做“参数展示”或“半离线记录”，不能完整重建 ISP 离线重放流程。

### 4.2 HV 离线重放缺失

原表中缺失了下列 HV 运行参数：

- `hventerstabdelay`
- `progmodedelay`
- `latchcycles`
- `togglevtg`
- `poweroffdelay`
- `resetdelayms`
- `resetdelayus`
- `hvleavestabdelay`
- `resetdelay`
- `chiperasepulsewidth`
- `chiperasepolltimeout`
- `chiperasetime`
- `programfusepulsewidth`
- `programfusepolltimeout`
- `programlockpulsewidth`
- `programlockpolltimeout`
- `synchcycles`
- `hvspcmdexedelay`

这些字段在 `hvproc.c` 对应的在线编程动作中，属于真正参与时序输出的参数。

## 5. 本次代码补充内容

### 5.1 扩展了 `AVR_MemSlot`

新增：

- `mode`
- `readback[2]`

目的：

- 对齐 avrdude 在线页写/字写命令构造所需字段

### 5.2 扩展了 `AVR_DeviceEntry`

新增：

- `runtime_group`

目的：

- 避免重写整张 392 项器件表
- 通过“共享运行参数组”补充离线所需的扩展参数

### 5.3 扩展了 `AVRPART`

新增：

- `predelay`
- `postdelay`
- `pollmethod`
- 全部 HV 运行时字段

目的：

- 让 `avrAggregateParams()` 展开后的结构，能覆盖在线编程真正用到的运行参数

### 5.4 在 `avrDeviceConst.c` 中新增共享运行时参数表

新增：

- `AVR_RuntimeGroup`
- `g_avrRuntimeGroups[]`

当前先加入 1 组共享默认值，来源如下：

- `predelay/postdelay`
  - 参考 avrdude `stk500v2.c` 中 `CMD_LEAVE_PROGMODE_ISP` 固定发送 `1/1`
- HV 典型时序字段
  - 参考 `avrdude-avr-init.xml` 中 classic AVR 的常见值

### 5.5 修正了共享 MEM 组默认值

为共享 `g_avrMemGroups[0]` 增加了：

- `FLASH mode = 0x41`
- `EEPROM mode = 0x04`
- 默认 `readback`

这一步主要是为了给未来 AVR 离线页写重放准备基础字段。

## 6. 本次对 avrOffLinePgm 的同步修正

### 6.1 新增 `ofp_make_leave_progmode_packet()`

使用新增的：

- `predelay`
- `postdelay`

来重建 `CMD_LEAVE_PROGMODE_ISP`

### 6.2 修正 `ofp_make_chip_erase_packet()`

原来：

- `pollMethod` 固定写死为 `0`

现在改为：

- 从 `AVRPART.pollmethod` 取值

这使得离线生成的擦除包与在线逻辑更一致。

## 7. 当前仍然存在的结构性限制

虽然这次已经把“明显缺失且在线实际用到”的参数补进来了，但当前 AVR 常量表仍有以下结构性限制：

### 7.1 `g_avrMemGroups[]` 已进一步提炼成 5 组

本次根据 `avrdude-avr-init.xml` 和在线函数实际使用字段，将 MEM 参数按行为提炼为 5 组：

- `0`: 默认 classic paged AVR，适用于 ATmega328P、ATtiny85 等常见页写器件
- `1`: AT90S2313 旧式 HVPP 字写/字节写器件
- `2`: AT90S2323/AT90S2343 旧式 HVSP 字写/字节写器件
- `3`: ATmega8 旧式页写 Flash + 字节写 EEPROM 器件
- `4`: ATtiny13/25/45/85 小页写 + HVSP 运行时器件族

分组表主要保存：

- `mode`
- `delay`
- `readsize`
- `readback[2]`
- fuse/signature/calibration 等固定区域大小

器件容量和 Flash 页大小仍从 `AVR_DeviceEntry` 展开后写回 `AVRPART.mem[]`，这样避免为每个容量型号重复建立 MEM 组。

### 7.2 `g_avrRuntimeGroups[]` 已进一步提炼成 4 组

本次将运行时参数提炼为：

- `0`: 默认 classic HVPP，例如 ATmega328P
- `1`: AT90S2313 旧式 HVPP
- `2`: AT90S2323/ATtiny13 系列 HVSP
- `3`: ATmega8 旧式 HVPP

这些组覆盖在线实际使用的：

- `predelay/postdelay/pollmethod`
- `hventerstabdelay`
- `latchcycles/togglevtg/poweroffdelay`
- `resetdelayms/resetdelayus/resetdelay`
- `chiperasepulsewidth/chiperasepolltimeout/chiperasetime`
- `programfuse/programlock` 相关 pulse/poll timeout
- `synchcycles/hvspcmdexedelay`

当前 `g_avrDeviceTable[]` 的历史数据大多没有填写 `runtime_group`，所以新增了一个按器件名称匹配的覆盖表，优先覆盖常见 AVR 器件族，后续如果重新生成全量表，可以直接填 `mem_group/runtime_group` 字段。

### 7.3 `g_avrOpGroups[]` 目前只有 1 组

这意味着：

- 默认假设 AVR ISP SPI 操作码几乎一致

对绝大多数 classic AVR 是成立的，但对：

- 新架构
- 非 ISP 主流路径
- 特殊页写器件

未必完全成立。

## 8. 结论

结论分 3 条：

1. 原 `avrDeviceConst` 并不满足完整 AVR 离线烧录所需的全部参数。
2. 缺失最关键的是：
   - `predelay/postdelay/pollmethod`
   - `mem.mode`
   - `mem.readback[2]`
   - HV 相关运行时参数
3. 本次已经把这些“在线实际会参与编程动作”的参数补进了 AVR 参数结构体和共享常量数组，并同步修正了 `avrOffLinePgm` 中直接依赖它们的接口。

## 9. 后续建议

如果下一步要继续把 AVR 离线烧录做实，建议按这个顺序推进：

1. 从 `avrdude-avr-init.xml` 继续提炼多组 `AVR_MemGroup`
2. 为 classic ISP / HVSP / HVPP 器件建立多组 `AVR_RuntimeGroup`
3. 在 AVR 离线执行模块中补齐：
   - `PROGRAM_FLASH_ISP`
   - `PROGRAM_EEPROM_ISP`
   - `PROGRAM_FUSE_ISP`
   - `PROGRAM_LOCK_ISP`
   - HVSP / PP 对应离线重放接口

这样 AVR 离线烧录链路才会从“可分析、可组包”升级到“可完整独立执行”。
