#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
修复编码问题并验证所有 .c .h 文件
1. 检查每个文件是否为有效的 GB2312
2. 检查第一个字符是否为有效 C 语法
3. 修复乱码
"""
import os
import re

BASE_DIR = r"e:\Codex\Project\Keil\Reference_Local\STM32F103VET6_BaseFrame"
SKIP_DIRS = {'OBJ', '.vscode', 'RTE'}
SKIP_FILES = {
    'stm32f10x.h', 'system_stm32f10x.h', 'startup_stm32f10x_hd.s',
    'FONT.H', 'oledfont.h',
}

# 有效 C 标识符或预处理指令开头
VALID_STARTS = set('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_#/*')

def should_skip(filepath):
    rel = os.path.relpath(filepath, BASE_DIR)
    for p in rel.split(os.sep)[:-1]:
        if p in SKIP_DIRS:
            return True
    return os.path.basename(filepath) in SKIP_FILES

def fix_file(filepath):
    if should_skip(filepath):
        return
    
    rel = os.path.relpath(filepath, BASE_DIR)
    
    with open(filepath, 'rb') as f:
        raw = f.read()
    
    # 尝试多种解码方式
    content = None
    for enc in ('gbk', 'gb2312', 'utf-8', 'utf-8-sig', 'latin-1'):
        try:
            decoded = raw.decode(enc)
            content = decoded
            break
        except:
            continue
    
    if content is None:
        print(f"  CANNOT DECODE: {rel}")
        return
    
    # 检查第一个有效字符
    stripped = content.lstrip()
    if stripped and stripped[0] not in VALID_STARTS:
        print(f"  BAD CHAR at byte 0 in {rel}: first printable char = {repr(stripped[0])}")
        # 尝试去掉前几个字节
        for i in range(1, min(5, len(raw))):
            try:
                decoded = raw[i:].decode('gbk')
                if decoded.lstrip() and decoded.lstrip()[0] in VALID_STARTS:
                    # 修复！去掉前 i 个字节
                    content = decoded
                    print(f"    -> Fixed by removing {i} leading byte(s)")
                    break
            except:
                continue
    
    # 重新编码为 GB2312
    try:
        new_raw = content.encode('gb2312')
    except:
        try:
            new_raw = content.encode('gbk', errors='replace')
        except:
            print(f"  ENCODE ERROR: {rel}")
            return
    
    if new_raw == raw:
        print(f"  OK: {rel}")
        return
    
    # 写回
    with open(filepath, 'wb') as f:
        f.write(new_raw)
    print(f"  FIXED: {rel}")

def main():
    print("=" * 60)
    print("检查并修复文件编码问题")
    print("=" * 60)
    
    all_files = []
    for root, dirs, files in os.walk(BASE_DIR):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in files:
            if f.endswith(('.c', '.h')):
                all_files.append(os.path.join(root, f))
    
    all_files.sort()
    for fp in all_files:
        fix_file(fp)
    
    print("=" * 60)
    print("完成")
    print("=" * 60)

if __name__ == '__main__':
    main()

