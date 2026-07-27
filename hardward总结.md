# Hardware_Config.h 引脚定义总结

> 按 PORTA ~ PORTE、bit0 ~ bit15 顺序排列
> 编码：GB2312

---

## PORTA

| 引脚 | IO方向 | 是否复用 | 宏定义名称 | 功能描述 |
|:----:|:------:|:--------:|-----------|---------|
| PA0  | -- | -- | -- | 未使用（Wake UP 功能引脚） |
| PA1  | 模拟输入 | 是 | HW_ADC2_IN1_VDD_FBACK | DUT VDD 开关后的工作电压（ADC2 通道，DUT工作后测量） |
| PA2  | 模拟输入 | 是 | HW_ADC1_IN2_VPP_MAIN_FBACK | DUT主电路5V升至12V后的电压（ADC1 通道，开机即可测量） |
| PA3  | 模拟输入 | 是 | HW_ADC1_IN3_USB_GOOD | VUSB->MOS管开关后电压测量点（ADC1 通道，开机即可测量） |
| PA4  | 模拟输入 | 是 | HW_ADC2_IN4_DUT_IVDD | DUT VDD 工作电流测量点（ADC2 通道，DUT工作后测量） |
| PA5  | 模拟输入 | 是 | HW_ADC2_IN5_DUT_IVPP | DUT VPP 工作电流测量点（ADC2 通道，DUT工作后测量） |
| PA6  | 模拟输入 | 是 | HW_ADC1_IN6_3V3_POWER_GOOD | STM32 MCU 工作电压测量点（ADC1 通道，开机即可测量） |
| PA7  | 模拟输入 | 是 | HW_ADC2_IN7_DUT_UVPP | DUT VPP 工作电压测量点（ADC2 通道，DUT工作后测量） |
| PA8  | 数字输出 | -- | HW_HANDLER_BUSY | Handler 接口 - 测试中/忙标志输出 |
| PA9  | 数字输入 | 是 | -- | UART1_RX |
| PA10 | 数字输出 | 是 | -- | UART1_TX |
| PA11 | -- | 是 | -- | USB-（USB_DP/DM） |
| PA12 | -- | 是 | -- | USB+（USB_DP/DM） |
| PA13 | -- | 是 | -- | SWDIO（调试接口） |
| PA14 | -- | 是 | -- | SWCLK（调试接口） |
| PA15 | -- | -- | -- | 未使用 |

---

## PORTB

| 引脚 | IO方向 | 是否复用 | 宏定义名称 | 功能描述 |
|:----:|:------:|:--------:|-----------|---------|
| PB0  | 数字输入 | -- | HW_GB2312_MISO | LCD12864 - GB2312 字库数据输入(MISO)，软件SPI，硬件2脚 |
| PB1  | 数字输出 | -- | HW_LCD12864_GB2312_SCK | LCD12864 - GB2312 字库时钟(SCK)/LCD12864时钟，软件SPI，硬件3脚、8脚 |
| PB2  | -- | -- | -- | 未使用 |
| PB3  | 数字IO | -- | HW_DUT_PIN5_DAT | DUT总线 PIN5 数据线（GPIO模拟） |
| PB4  | 数字IO | -- | HW_DUT_PIN4_DAT | DUT总线 PIN4 数据线（GPIO模拟） |
| PB5  | 数字IO | -- | HW_DUT_PIN7_DAT | DUT总线 PIN7 数据线（GPIO模拟） |
| PB6  | 数字IO | -- | HW_DVR_VDD_IIC_SDA | VDD数控电位器 IIC 总线 SDA，软件IIC（常压电源） |
| PB7  | 数字输出 | -- | HW_DVR_VDD_IIC_SCL | VDD数控电位器 IIC 总线 SCL，软件IIC（常压电源） |
| PB8  | -- | -- | -- | 未使用 |
| PB9  | 数字输出 | -- | HW_BEEP | 蜂鸣器 |
| PB10 | 数字输出 | -- | HW_LCD12864_RST | LCD12864 复位(RST)，硬件11脚 |
| PB11 | -- | -- | -- | 未使用 |
| PB12 | -- | -- | -- | 未使用 |
| PB13 | 数字输出 | 是 | HW_SPI2_SCK | EEPROM/Flash - SPI2 时钟线（硬件SPI复用） |
| PB14 | 数字输入 | 是 | HW_SPI2_SDI | EEPROM/Flash - SPI2 数据输入(MISO)（硬件SPI复用） |
| PB15 | 数字输出 | 是 | HW_SPI2_SDO | EEPROM/Flash - SPI2 数据输出(MOSI)（硬件SPI复用） |

---

## PORTC

| 引脚 | IO方向 | 是否复用 | 宏定义名称 | 功能描述 |
|:----:|:------:|:--------:|-----------|---------|
| PC0  | 数字输出 | -- | HW_LED_ACTIVE | 三色LED - 有源指示（DS0），也用于烧录动作指示 `HWPIN_LED` |
| PC1  | 数字输出 | -- | HW_LED_RESET | 三色LED - 复位指示（DS2） |
| PC2  | 数字输出 | -- | HW_LED_HALT | 三色LED - 暂停指示（DS1） |
| PC3  | 数字输出 | -- | HW_USB_ON | USB VBUS +5V 电源开关控制（1：打开；0：关闭） |
| PC4  | 模拟输入 | 是 | HW_ADC1_IN14_VDD_MAIN | DUT主电路12V降至5V后的工作电压（ADC1 通道，开机即可测量） |
| PC5  | 数字输出 | -- | HW_LCD12864_GB2312_MOSI | LCD12864 / GB2312 数据输入(MOSI)，软件SPI，硬件1脚、9脚 |
| PC6  | 数字输出 | -- | HW_HANDLER_OK | Handler 接口 - 测试合格输出 |
| PC7  | 数字输出 | -- | HW_HANDLER_NG | Handler 接口 - 测试失效输出 |
| PC8  | 数字IO | 是 | HW_SDIO_DAT0 | SD卡 DAT0（SDIO外设控制，双向） |
| PC9  | 数字IO | 是 | HW_SDIO_DAT1 | SD卡 DAT1（SDIO外设控制，双向） |
| PC10 | 数字IO | 是 | HW_SDIO_DAT2 | SD卡 DAT2（SDIO外设控制，双向） |
| PC11 | 数字IO | 是 | HW_SDIO_DAT3 | SD卡 DAT3（SDIO外设控制，双向） |
| PC12 | 数字输出 | 是 | HW_SDIO_CLK | SD卡 时钟（SDIO外设控制） |
| PC13 | -- | -- | -- | 未使用（RTC） |
| PC14 | -- | -- | -- | 未使用（RTC晶振） |
| PC15 | -- | -- | -- | 未使用（RTC晶振） |

---

## PORTD

| 引脚 | IO方向 | 是否复用 | 宏定义名称 | 功能描述 |
|:----:|:------:|:--------:|-----------|---------|
| PD0  | 数字输出 | -- | HW_HANDLER_UD | Handler 接口 - 上/下拉电阻控制 |
| PD1  | 数字输入 | -- | HW_HANDLER_START | Handler 接口 - "开始测试"信号输入 |
| PD2  | 数字IO | 是 | HW_SDIO_CMD | SD卡 命令线（SDIO外设控制，双向：主机发命令→设备发响应） |
| PD3  | 数字输出 | -- | HW_DUT_PIN4_CTRL | DUT总线 PIN4 方向控制（1=输出, 0=输入） |
| PD4  | 数字输出 | -- | HW_DUT_PIN5_CTRL | DUT总线 PIN5 方向控制（1=输出, 0=输入） |
| PD5  | 数字输出 | -- | HW_DVR_VPP_IIC_SCL | VPP数控电位器 IIC 总线 SCL，软件IIC（高压电源） |
| PD6  | 数字IO | -- | HW_DVR_VPP_IIC_SDA | VPP数控电位器 IIC 总线 SDA，软件IIC（高压电源） |
| PD7  | -- | -- | -- | 未使用 |
| PD8  | 数字输出 | -- | HW_FLASH_WP | Flash 写保护控制 |
| PD9  | 数字输出 | -- | HW_FLASH_CS | Flash 片选控制 |
| PD10 | 数字输出 | -- | HW_SPI_EEPROM_WP | EEPROM 写保护控制 |
| PD11 | 数字输出 | -- | HW_SPI_EEPROM_CS | EEPROM 片选控制 |
| PD12 | 数字输入 | -- | HW_BTN_DOWN | 按键 - 下 |
| PD13 | 数字输入 | -- | HW_BTN_UP | 按键 - 上 |
| PD14 | 数字输入 | -- | HW_BTN_BACK | 按键 - 返回 |
| PD15 | 数字输入 | -- | HW_BTN_ENTER | 按键 - 确认 |

---

## PORTE

| 引脚 | IO方向 | 是否复用 | 宏定义名称 | 功能描述 |
|:----:|:------:|:--------:|-----------|---------|
| PE0  | 数字输出 | -- | HW_DUT_PIN6_CTRL | DUT总线 PIN6 方向控制（1=输出, 0=输入） |
| PE1  | -- | -- | -- | 未使用 |
| PE2  | 数字IO | -- | HW_DUT_PIN6_DAT | DUT总线 PIN6 数据线（GPIO模拟） |
| PE3  | -- | -- | -- | 未使用 |
| PE4  | 数字输出 | -- | HW_DUT_PIN7_CTRL | DUT总线 PIN7 方向控制（1=输出, 0=输入） |
| PE5  | 数字IO | -- | HW_DUT_PIN8_DAT | DUT总线 PIN8 数据线（GPIO模拟） |
| PE6  | 数字输出 | -- | HW_DUT_PIN8_CTRL | DUT总线 PIN8 方向控制（1=输出, 0=输入） |
| PE7  | 数字输出 | -- | HW_GB2312_CS | LCD12864 - GB2312 字库片选(CS)，硬件4脚 |
| PE8  | 数字输出 | -- | HW_LCD12864_RS | LCD12864 数据/命令选择(DC)，硬件10脚 |
| PE9  | -- | -- | -- | 未使用 |
| PE10 | 数字输出 | -- | HW_LCD12864_CS | LCD12864 片选(CS)，硬件12脚 |
| PE11 | -- | -- | -- | 未使用 |
| PE12 | 数字输出 | -- | HW_DUT_VPP_VH_ON | DUT VPP 高压输出打开（1=输出, 0=关闭） |
| PE13 | 数字输出 | -- | HW_DUT_VPP_VL_ON | DUT VPP 高压接地打开（1=接地, 0=关闭） |
| PE14 | 数字输出 | -- | HW_DUT_VDD_VH_ON | DUT VDD 输出打开（1=输出, 0=关闭） |
| PE15 | 数字输出 | -- | HW_DUT_VDD_VL_ON | DUT VDD 接地打开（1=接地, 0=关闭） |

---

## 补充说明

### DUT 总线引脚映射关系

| DUT引脚 | ICSP模式 | ISP 模式 | JTAG 模式 | DW 模式 | HVSER 模式 |
|:-------:|:--------:|:--------:|:---------:|:-------:|:----------:|
| PIN1    | MCLR     | --       | --        | --      | RESET      |
| PIN2    | VDD      | VDD      | VDD       | VDD     | VDD        |
| PIN3    | GND      | GND      | GND       | GND     | GND        |
| PIN4    | ICSPDAT  | MISO     | TDO       | --      | SDO        |
| PIN5    | ICSPCLK  | SCK      | TCK       | DW      | SCK        |
| PIN6    | AUX      | RESET    | RESET     | --      | SDI        |
| PIN7    | --       | MOSI     | TDI       | --      | SII        |
| PIN8    | --       | --       | TMS       | --      | --         |

### DUT 总线控制信号与引脚

| DUT引脚 | 方向控制(CTRL) | 数据线(DAT) |
|:-------:|:--------------:|:-----------:|
| PIN4    | PD3            | PB4         |
| PIN5    | PD4            | PB3         |
| PIN6    | PE0            | PE2         |
| PIN7    | PE4            | PB5         |
| PIN8    | PE6            | PE5         |

### VPP / VDD 开关控制

| 功能 | 宏定义 | 引脚 | 说明 |
|:----:|--------|:----:|------|
| VPP高压输出 | HW_DUT_VPP_VH_ON | PE12 | 1=输出打开 |
| VPP高压接地 | HW_DUT_VPP_VL_ON | PE13 | 1=接地打开 |
| VDD输出 | HW_DUT_VDD_VH_ON | PE14 | 1=输出打开 |
| VDD接地 | HW_DUT_VDD_VL_ON | PE15 | 1=接地打开 |

### ADC 通道分类

**开机即可测量（ADC1）：**
- PA2: VPP 主电路电压(5V→12V)
- PA3: USB 电源电压
- PA6: 3.3V MCU电压
- PC4: VDD主电路电压(12V→5V)

**DUT工作后测量（ADC2）：**
- PA1: DUT VDD电压
- PA4: DUT VDD电流
- PA5: DUT VPP电流
- PA7: DUT VPP电压

### 接口资源分配

| 外设接口 | 使用的引脚 | 功能 |
|---------|-----------|------|
| SPI2（硬件复用） | PB13(SCK), PB14(SDI), PB15(SDO) | EEPROM / Flash |
| SDIO（硬件复用） | PC8~PC12, PD2 | SD卡 |
| ADC1（硬件复用） | PA2, PA3, PA6, PC4 | 开机电压监测 |
| ADC2（硬件复用） | PA1, PA4, PA5, PA7 | DUT工作电压/电流监测 |
| 软件IIC (VPP) | PD5(SCL), PD6(SDA) | VPP数控电位器（GPIO模拟IIC） |
| 软件IIC (VDD) | PB6(SDA), PB7(SCL) | VDD数控电位器（GPIO模拟IIC） |
| 软件SPI (LCD) | PC5(MOSI), PB1(SCK), PB0(MISO) | LCD12864 / GB2312 字库（GPIO模拟SPI） |

### USB 硬件中断流程

```
STM32 USB 硬件中断
  └── CTR_LP() (USB 库 ISR 入口)
        └── 解析 SETUP 包
              │
              ├── SET_REPORT (主机 → 设备)
              │     └── UsbHidDev_Data_Setup()
              │           └── 设置 CopyRoutine = UsbHidDev_SetReportData
              │                 (USB 数据阶段将主机数据复制到 s_reportBuf)
              │           └── 等待 Status IN
              │                 └── UsbHidDev_Status_In()   ← 仍在 USB ISR 中!
              │                       └── HID_Rx_Store()
              │                             └── 搜索 STK_STX (0x1B) 帧头
              │                             └── stkSetRxChar(data[i]) 逐字节送入
              │                             └── stkPoll() 检查帧完整性
              │                                   └── stkEvaluateRxMessage()
              │                                         └── [解析命令 + 执行 + 排队响应]
              │                                         └── stkSetTxMessage()
              │
              └── GET_REPORT (设备 → 主机)
                    └── UsbHidDev_Data_Setup()
                          └── HID_GetReport_Buffer()
                                └── 从 TX 缓冲中读取已排队的响应字节
                                └── 通过 USB 数据阶段发送给主机
```

### ICSP 硬件中断参与分析

ICSP 编程操作期间，以下硬件事件通过中断参与：

| 中断源 | 是否参与 ICSP 过程 | 说明 |
|:-------|:-----------------:|:-----|
| **USB 控制传输** | 是 | SET_REPORT/GET_REPORT 在 ISR 中独立完成，不受 ICSP 编程阻塞 |
| **SysTick** | 是 | `delay_ms()` / `ICSP_DELAY_US()` 使用忙等（软件循环计数），不依赖 SysTick |
| **DMA (ADC)** | 是 | ADC1/ADC2 电压/电流监测通过 DMA 持续工作 |
| **EXTI (按键)** | 是 | 按键中断独立运行 |
| **SDIO** | 否 | SD 卡操作在 `fatfs` 任务中轮询调用 |
| **SPI2 (Flash/EEPROM)** | 否 | 轮询接口，非中断驱动 |

**关键发现**: ICSP 编程虽然长时间阻塞主循环（长达数秒的忙等操作），但由于 USB 通信路径完全在 ISR 上下文中独立完成，ICSP 编程期间主机仍可正常发送查询命令并收到响应。ISR 的执行不受主循环阻塞影响。

### ICSP 时序宏定义分析

ICsP 的微秒级时序通过以下宏实现，位于 `icsp.h`：

```c
#define ICSP_DELAY_US(n)  do{ volatile uint32_t _i_ = (uint32_t)(n) * 12UL; while(_i_--); }while(0)
#define ICSP_CLK_DELAY    ICSP_DELAY_US(1)
```

| 特性 | 说明 |
|:-----|:-----|
| 实现方式 | 软件忙等循环计数（volatile 阻止编译器优化） |
| 时钟依赖 | 72MHz 主频，乘以 12UL 系数 |
| SysTick 依赖 | **无** — 纯 CPU 周期循环 |
| 中断影响 | 若发生 ISR，循环计数会偏大 → 延时比预期长 |
| 确定性 | 关中断时最精确；开中断时有 ±N×ISR_cycles 抖动 |

**现实场景**: 由于 ICSP 编程是串行命令交互过程，正常操作期间 USB 几乎不会发 SET_REPORT（主机在等待上一命令的响应），因此 ISR 对 `ICSP_DELAY_US` 的干扰在大多数情况下可忽略。唯一可能产生中断的是 SysTick（1ms 周期）和 ADC DMA 完成中断，影响有限。

### GPIO 输出延时对比

| 操作 | 代码 | 延时 | 备注 |
|:-----|:-----|:----:|:-----|
| 普通引脚输出 | `PORT_OUT(pin)=1` | ~12ns | 单周期位带操作 |
| 开时钟+输出 | `PORT_RCC_CLK(pin); PORT_OUT(pin)=1` | ~100ns | RCC 操作需等待 APB2 总线 |
| ICSP 翻转 | `ICSP_CLK_H(); ICSP_CLK_DELAY; ICSP_CLK_L()` | ~1.5μs | 含 1μs 忙等 |
| VPP/VDD 开关 | `ICSP_VPP_ON()` | ~200ns | 开时钟 + 输出 |

## 存储器配置
### EEPROM
      FT25C64A（64Kbit / 8KB）

### Flash
      FM25W32，容量 32 Mbit = 4 MB
