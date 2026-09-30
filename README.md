# STM32F103 PIC / AVR Programmer Firmware

基于 `STM32F103VE` 的 PIC 与 AVR 通用烧录器下位机固件，面向在线烧录、离线回放及烧录器硬件调试场景。

项目运行于 STM32F103 的 App 区，App 起始地址为 `0x0800C000`；前置 Flash 区由独立 Bootloader 管理，用于安全更新 App 固件。

## 主要功能

- AVR 器件在线烧录：支持 ISP、HVSP、HVPP 等烧录链路。
- PIC 器件在线烧录：当前覆盖部分 PIC10 / PIC12 / PIC16 器件，通过 ICSP 链路完成擦除、写入、读取与校验。
- 离线烧录回放：将上位机的 STK500 编程帧记录到外部 SPI Flash，脱离 PC 后由按键或机械手触发回放。
- 离线项目管理：支持离线包保存、CRC 校验、激活项目切换、离线包导出和与原始 HEX 文件比对。
- 器件型号识别与切换：上位机通过器件身份参数指定 AVR/PIC 架构与器件索引，下位机据此加载对应参数。
- 电源控制与测量：支持 VDD、VPP 设置、ADC 采集、MCP4017 数控电位器整定及 EEPROM 校准参数保存。
- 机械手接口：支持读取和配置 Handler 参数、统计测试总数/良品数/不良品数并复位统计值。
- 固件更新：App 可设置 Bootloader 更新标志并软复位，随后由 Bootloader 接收和更新 App 镜像。
- 滚码功能：规划中，当前尚未实现。

## 系统架构

```text
PC 上位机
    |
    | USB 复合设备：HID + CDC + WinUSB
    v
STM32F103 App Firmware @ 0x0800C000
    |
    +-- STK500 协议适配层
    |     +-- AVR ISP / HVSP / HVPP
    |     +-- PIC ICSP
    |
    +-- 离线录制与回放
    |     +-- 外部 SPI Flash：离线项目包
    |     +-- EEPROM：激活项目、校准参数、机械手参数及统计记录
    |
    +-- 电源、ADC、LED、蜂鸣器、LCD、按键、Handler
    |
    +-- Bootloader 协作接口
```

## USB 通信

设备连接 PC 后枚举为 USB 复合设备：

- `USB HID`：主要用于 STK500 编程协议及兼容 avrdude 的在线烧录链路。
- `USB CDC`：用于串口类通信与诊断扩展。
- `WinUSB`：用于 Bootloader 更新、机械手参数配置、统计读取等专用二进制协议。

协议层负责帧校验、命令分发、在线/录制/离线包读取模式切换，以及 AVR 与 PIC 编程链路的统一协调。

## 工作模式

| 模式 | 说明 |
|---|---|
| 默认或 `-x workmode=1` | 在线模式，实际操作目标器件。 |
| `-x workmode=2` | 录制模式，仅记录上位机编程数据到外部 Flash，不操作目标器件。 |
| `-x workmode=4` | 离线包读取/校验模式，从当前激活离线包虚拟读取数据，不操作目标器件、不改写离线包。 |
| 内部回放模式 | 由按键或 Handler 触发，从外部 Flash 的激活离线包执行真实烧录。 |

在模式 4 中，包内已有写入数据按记录内容返回；未写入区域按目标器件的擦除态返回，例如 PIC 程序区、User ID 和 Config 按器件字宽处理，EEPROM 返回 `0xFF`。

## Bootloader 与 App 更新

Bootloader 工程为 `STM32F103VET6_Bootloader_V1`，位于 App 前方 Flash 区域。

更新流程如下：

1. PC 更新程序向 App 发送固件更新请求。
2. App 在 Boot 控制区写入更新标志及镜像状态。
3. PC 再发送 RESET 指令，MCU 软件复位。
4. Bootloader 启动后检查更新标志。
5. 检测到有效更新请求时，Bootloader 进入 App 镜像接收和写入流程。
6. 未检测到更新请求时，Bootloader 直接校验并跳转至 `0x0800C000` 的 App。
7. App 启动后确认运行状态，防止异常更新状态长期滞留。

## 配套上位机项目

### avrdude

主烧录上位机，基于开源 avrdude 修改。

- 使用 STK500 命令体系与下位机通信。
- 保持 AVR 烧录能力，并扩展支持部分 PIC10 / PIC12 / PIC16 器件。
- 支持在线烧录、离线录制、激活离线包读取与 HEX 校验。
- 支持 VDD、VPP 设置与回读等烧录器扩展参数。

项目目录：`E:\wangjunhua\Project\AvrProgrammer\avrdude`

### AVRDUDESS-2.20

基于 C# 的 avrdude 图形前端，主要用于 AVR 在线烧录和日常调试。

- 提供桌面操作界面。
- 通过命令行调用 avrdude。
- 适合快速组织烧录参数、执行读写校验和观察 avrdude 输出。

项目目录：`E:\wangjunhua\Project\AvrProgrammer\AVRDUDESS-2.20`

### STM32F103VET6_BootLoader_PC

基于 Python 的 Bootloader 与设备管理上位机。

- 更新本项目 App 固件。
- 通过 USB 复合设备的专用协议通信。
- 配置机械手参数。
- 读取、复位测试统计数据。
- 设置转换芯片型号等设备级参数。

项目目录：`E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_BootLoader_PC`

### STM32F103_Programmer_UART_Debuger

基于 Python 和 UART 的内部硬件调试工具，不面向客户发布。

- 调试 PCB 硬件和定位问题。
- 测试 VDD、VPP、ADC、Flash、EEPROM、LED、蜂鸣器及通信链路。
- 初始化 PCB 校准参数。
- 扫描 MCP4017 与输出电压关系，并拟合、写入 EEPROM 校准参数。
- 导出当前离线包、校验包 CRC，并与原始 HEX 文件比对。

项目目录：`E:\Codex\Project\Keil\Reference_Local\STM32F103_PC_Mast_Uart_Debuger`

## 工程目录概览

- `USER/`：主循环、STK500 协议、离线包管理、调试命令与业务逻辑。
- `PROGRAMMER/`：AVR ISP/HVSP/HVPP、PIC ICSP 及器件参数表。
- `HARDWARE/`：ADC、电源、Flash、EEPROM、LCD、LED、UART、SPI、I2C 等驱动。
- `USB/`：USB HID、CDC、WinUSB 协议栈与设备描述符。
- `HANDLER/`：机械手接口、状态机、统计和动作控制。
- `MDK-ARM/`：Keil MDK 工程文件。
- `tools/`：构建辅助工具，例如 App 构建时间生成脚本。

## 开发说明

- 主要开发环境为 Keil MDK-ARM。
- 固件历史文件中包含 GB2312 编码内容，修改时应避免无关编码转换。
- `MDK-ARM.vscode/` 为本地构建和编辑器生成目录，不应提交到 Git。
- 修改离线存储格式、Boot 控制记录或 EEPROM 地址前，应同时评估与 Bootloader、PC 工具及既有离线包的兼容性。
