#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
为所有 .c .h 文件中开头没有中文注释的，添加中文用途注释。
同时为FAT32相关文件添加额外中文注释。
"""
import os

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
SKIP_DIRS = {'OBJ', 'DebugConfig', 'RTE', '.git', '__pycache__'}
EXTS = {'.c', '.h'}

# 文件用途映射表 - 基于文件名和目录确定文件用途
FILE_PURPOSE = {
    # ===== FAT32 文件 =====
    'diskio.c': '底层磁盘I/O接口实现 - 桥接FatFs文件系统与SD卡SDIO驱动',
    'diskio.h': '底层磁盘I/O模块头文件 - 定义磁盘操作函数原型和状态常量',
    'ff.c': 'FatFs FAT文件系统核心实现（精简版FAT32）- 提供文件读写/目录操作等API',
    'ff.h': 'FatFs FAT文件系统API定义头文件 - 定义FATFS/FIL等核心结构体和函数原型',
    'ffconf.h': 'FatFs文件系统配置文件 - 开启/关闭功能模块（适用于STM32F103+SD卡）',
    'integer.h': 'FatFs整数类型定义头文件 - 跨平台整型类型映射（BYTE/WORD/DWORD等）',
    
    # ===== 根目录文件 =====
    'main.c': '主程序入口 - 系统初始化/主循环/调试任务调度',
    'Debug.c': '调试接口实现 - 串口命令解析与调试功能处理',
    'Debug.h': '调试接口头文件 - 定义调试命令枚举和调试任务函数',
    
    # ===== PROGRAMMER 目录 =====
    'hvproc.c': '高压编程器控制实现 - DUT编程/擦除/校验时序控制',
    'hvproc.h': '高压编程器头文件 - 定义高压编程接口函数原型',
    'isp.c': 'ISP在线编程实现 - 通过SPI或ICSP协议对MCU编程',
    'isp.h': 'ISP在线编程头文件 - 定义ISP编程接口函数原型',
    
    # ===== USB 目录 =====
    'testUsbHid.c': 'USB HID通信测试实现 - 用于测试HID数据收发',
    'hw_USB_config.c': 'USB硬件配置实现 - USB外设初始化与配置',
    'hw_USB_config.h': 'USB硬件配置头文件 - USB引脚/时钟/中断配置',
    'platform_config.h': '平台配置头文件 - 硬件平台相关的引脚和资源定义',
    'stm32f10x_it.c': 'STM32F10x中断服务例程 - USB相关中断处理',
    'usb_conf.h': 'USB配置头文件 - USB端点/缓冲区/传输配置',
    'usb_desc.c': 'USB描述符实现 - 设备/配置/接口/HID描述符定义',
    'usb_desc.h': 'USB描述符头文件 - 描述符结构体和常量定义',
    'usb_hid_user.c': 'USB HID用户层实现 - HID报告收发与应用层接口',
    'usb_hid_user.h': 'USB HID用户层头文件 - HID用户接口函数和结构体定义',
    'usb_istr.c': 'USB中断服务路由实现 - USB中断向量分发处理',
    'usb_istr.h': 'USB中断服务头文件 - 中断处理函数声明',
    'usb_prop.c': 'USB属性处理实现 - USB标准设备请求处理',
    'usb_prop.h': 'USB属性处理头文件 - 属性请求处理接口',
    'usb_pwr.c': 'USB电源管理实现 - USB挂起/恢复/电源状态控制',
    'usb_pwr.h': 'USB电源管理头文件 - 电源管理接口函数和状态定义',
    'usb_core.c': 'USB核心驱动实现 - USB协议引擎/令牌处理/传输管理',
    'usb_core.h': 'USB核心驱动头文件 - 核心USB协议函数和常量',
    'usb_def.h': 'USB标准定义头文件 - USB协议标准常量和结构体定义',
    'usb_init.c': 'USB初始化实现 - USB库初始化与全局状态复位',
    'usb_init.h': 'USB初始化头文件 - 初始化函数外部声明',
    'usb_int.c': 'USB中断驱动实现 - 正确/错误传输中断处理',
    'usb_int.h': 'USB中断驱动头文件 - 中断处理函数声明',
    'usb_lib.h': 'USB库主头文件 - USB库所有模块的头文件集合',
    'usb_mem.c': 'USB内存管理实现 - USB缓冲区读写操作',
    'usb_mem.h': 'USB内存管理头文件 - 缓冲区操作函数声明',
    'usb_regs.c': 'USB寄存器操作实现 - USB外设寄存器读写封装',
    'usb_regs.h': 'USB寄存器操作头文件 - 寄存器地址定义和操作宏',
    'usb_type.h': 'USB类型定义头文件 - USB库中使用的自定义类型定义',
    
    # ===== USER 目录 =====
    'sdCardUser.c': 'SD卡用户层接口实现 - 文件读写/创建/删除等高层操作',
    'sdCardUser.h': 'SD卡用户层接口头文件 - 文件操作函数原型定义',
    'Stk500Protocol.c': 'STK500编程协议实现 - 兼容Arduino的STK500通信协议',
    'Stk500Protocol.h': 'STK500协议头文件 - 协议常量/命令码/函数原型',
    
    # ===== HARDWARE 各模块 =====
    'adc.c': 'ADC驱动实现 - ADC1多通道扫描/数据采集和电压计算',
    'adc.h': 'ADC驱动头文件 - 通道定义/分压系数/采样参数配置',
    'beep.c': '蜂鸣器驱动实现 - 蜂鸣器控制',
    'beep.h': '蜂鸣器驱动头文件 - 蜂鸣器初始化与控制接口',
    'dma.c': 'DMA驱动实现 - DMA传输配置（用于ADC/USART等外设）',
    'dma.h': 'DMA驱动头文件 - DMA通道配置和传输函数',
    'dutBus.c': 'DUT总线控制实现 - 编程器引脚切换/电平控制',
    'dutBus.h': 'DUT总线头文件 - DUT引脚定义和总线控制接口',
    'eeprom.c': 'EEPROM驱动实现 - 24Cxx系列I2C EEPROM读写',
    'eeprom.h': 'EEPROM驱动头文件 - EEPROM操作函数和地址定义',
    'exti.c': '外部中断驱动实现 - EXTI中断配置与按键中断处理',
    'exti.h': '外部中断驱动头文件 - 外部中断初始化接口',
    'flash.c': 'SPI Flash驱动实现 - Flash芯片扇区擦除/读写操作',
    'flash.h': 'SPI Flash驱动头文件 - Flash操作函数和命令定义',
    'Hardware_Config.h': '硬件引脚配置头文件 - 统一定义所有外设的引脚映射和功能开关',
    'FONT.H': 'LCD字库头文件 - 英文字符点阵字库数据',
    'ILI93xx.c': 'ILI93xx LCD驱动实现 - TFT液晶屏初始化/画点/显示控制',
    'lcd.h': 'LCD驱动头文件 - LCD分辨率/颜色/显示函数定义',
    'FontGB2312.c': 'GB2312中文字库实现 - 汉字点阵字库数据（16x16点阵）',
    'FontGB2312.h': 'GB2312字库头文件 - 中文字库结构和访问函数',
    'Lcd12864.c': 'LCD12864驱动实现 - 12864点阵液晶屏驱动/显示/画图',
    'Lcd12864.h': 'LCD12864驱动头文件 - 显示函数和绘图接口',
    'Lcd12864Bmp.h': 'LCD12864位图头文件 - 位图数据定义（LOGO图标等）',
    'iicSoftware.c': '软件I2C驱动实现 - GPIO模拟I2C时序/读写操作',
    'iicSoftware.h': '软件I2C驱动头文件 - I2C总线操作函数',
    'key.c': '按键驱动实现 - 按键扫描/消抖/长按检测',
    'key.h': '按键驱动头文件 - 按键值定义和扫描接口',
    'led.c': 'LED驱动实现 - 状态指示灯控制',
    'led.h': 'LED驱动头文件 - LED初始化与控制接口',
    'MCP4017_VDD.c': 'VDD数控电位器驱动实现 - MCP4017控制DUT供电电压',
    'MCP4017_VDD.h': 'VDD数控电位器头文件 - 电压设定与控制接口',
    'MCP4017_VPP.c': 'VPP数控电位器驱动实现 - MCP4017控制DUT编程电压',
    'MCP4017_VPP.h': 'VPP数控电位器头文件 - 电压设定与控制接口',
    'power.c': '电源管理实现 - DUT电源/VPP/VDD开关控制',
    'power.h': '电源管理头文件 - 电源通道控制接口',
    'rtc.c': 'RTC实时时钟驱动实现 - 日历/时间设置与读取',
    'rtc.h': 'RTC实时时钟头文件 - 时间日期读写接口',
    'sdcard.c': 'SD卡底层驱动实现 - SDIO协议SD卡初始化/读写',
    'sdcard.h': 'SD卡底层驱动头文件 - SD卡状态定义和操作函数',
    'spi.c': 'SPI驱动实现 - SPI1/SPI2初始化与收发操作',
    'spi.h': 'SPI驱动头文件 - SPI配置参数和接口函数',
    'timer.c': '定时器驱动实现 - 定时器初始化/PWM生成/计时功能',
    'timer.h': '定时器驱动头文件 - 定时器配置和参数定义',
    'wdg.c': '看门狗驱动实现 - 独立看门狗IWDG复位控制',
    'wdg.h': '看门狗驱动头文件 - 看门狗初始化与喂狗接口',
    'wkup.c': '待机唤醒驱动实现 - 低功耗待机模式与唤醒控制',
    'wkup.h': '待机唤醒头文件 - 唤醒检查与待机模式接口',
    
    # ===== SYSTEM 目录 =====
    'delay.c': '微秒/毫秒延时实现 - SysTick定时器延时函数',
    'delay.h': '延时驱动头文件 - 延时函数声明与参数配置',
    'sys.c': '系统核心功能实现 - GPIO操作宏底层/中断分组/时钟配置',
    'sys.h': '系统核心头文件 - GPIO位操作宏/类型定义/系统函数',
    'stm32f10x.h': 'STM32F10x外设寄存器定义 - CPU寄存器地址映射和位定义',
    'system_stm32f10x.h': '系统时钟配置头文件 - STM32F10x时钟树配置参数',
    'startup_stm32f10x_hd.s': 'STM32F10x启动文件 - 中断向量表/堆栈初始化/Reset处理',
    'usart.c': '串口驱动实现 - USART1初始化/发送/接收/中断处理',
    'usart.h': '串口驱动头文件 - 串口配置参数和函数声明',
    'readme.txt': '系统目录说明文件 - delay/sys/usart模块说明',
}

def get_purpose(filepath, filename):
    """根据文件名获取用途说明"""
    if filename in FILE_PURPOSE:
        return FILE_PURPOSE[filename]
    # 如果不在映射表中，基于所在的目录生成描述
    rel = os.path.relpath(filepath, PROJECT_ROOT)
    dir_part = os.path.dirname(rel)
    # 根据文件扩展名判断
    if filename.endswith('.c'):
        return f'{dir_part}/{filename} - 模块实现文件'
    elif filename.endswith('.h'):
        return f'{dir_part}/{filename} - 头文件定义'
    return f'{filename} - 源文件'

def add_comment_to_file(filepath):
    """为文件添加中文头部注释"""
    rel = os.path.relpath(filepath, PROJECT_ROOT)
    filename = os.path.basename(filepath)
    
    try:
        with open(filepath, 'rb') as f:
            raw = f.read()
        text = raw.decode('gb2312')
    except Exception as e:
        return f"  错误: 无法读取文件 - {e}"
    
    lines = text.split('\r\n')
    
    # 检查前6行是否有中文
    has_cn = False
    for line in lines[:6]:
        for c in line:
            if ord(c) > 127:
                has_cn = True
                break
        if has_cn:
            break
    
    if has_cn:
        return None  # 已有中文注释，跳过
    
    purpose = get_purpose(filepath, filename)
    
    # 找到插入点
    # 跳过 #ifndef 守卫行和空行
    insert_idx = 0
    while insert_idx < len(lines):
        line = lines[insert_idx]
        stripped = line.strip()
        # 跳过注释块结束标记
        if stripped.startswith('*/'):
            # 找到注释块结束，在其后插入
            insert_idx += 1
            # 跳过空行
            while insert_idx < len(lines) and lines[insert_idx].strip() == '':
                insert_idx += 1
            break
        # 跳过 #ifndef 和 #define 守卫
        if stripped.startswith('#ifndef') or stripped.startswith('#define') or stripped == '':
            insert_idx += 1
            continue
        # 遇到 #include 或其他代码
        if stripped.startswith('#include') or stripped.startswith('/*') or stripped.startswith('*'):
            # 如果是注释或include，在第一个非守卫行前插入
            break
        break
    
    # 如果文件以 #ifndef 开头，在它之前插入注释
    first_real_line = 0
    for i, line in enumerate(lines):
        s = line.strip()
        if s and not s.startswith('#ifndef') and not s.startswith('#define'):
            first_real_line = i
            break
    
    # 更好的策略：在文件最开头插入
    comment_block = [
        f'/*',
        f' * {purpose}',
        f' */',
        f''
    ]
    
    # 插入注释
    new_lines = comment_block + lines
    
    # 重新组合
    new_text = '\r\n'.join(new_lines)
    
    try:
        new_raw = new_text.encode('gb2312')
    except:
        try:
            new_raw = new_text.encode('gbk')
        except:
            return f"  错误: 编码失败"
    
    # 写回
    try:
        with open(filepath, 'wb') as f:
            f.write(new_raw)
    except:
        return f"  错误: 写入失败"
    
    return f"  已添加注释: {purpose}"

def main():
    print("=" * 70)
    print("为所有 .c .h 文件添加中文头部注释")
    print("=" * 70)
    
    modified = 0
    skipped_cn = 0
    total = 0
    errors = []
    
    for root, dirs, files in os.walk(PROJECT_ROOT):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in sorted(files):
            ext = os.path.splitext(f)[1].lower()
            if ext not in EXTS:
                continue
            fp = os.path.join(root, f)
            total += 1
            result = add_comment_to_file(fp)
            rel = os.path.relpath(fp, PROJECT_ROOT)
            
            if result is None:
                skipped_cn += 1
            else:
                print(f"[{rel}]")
                print(result)
                modified += 1
    
    print()
    print("=" * 70)
    print(f"总文件: {total}")
    print(f"已有中文注释(跳过): {skipped_cn}")
    print(f"已添加中文注释: {modified}")
    if errors:
        print(f"错误: {len(errors)}")
        for e in errors:
            print(f"  {e}")
    print("=" * 70)

if __name__ == '__main__':
    main()