#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
批量处理所有 .c .h 文件：
1. 删除版权类注释头（正点原子版权信息、分隔线、版本历史等）
2. 转换为 GB2312 编码
"""

import os
import re

BASE_DIR = r"e:\Codex\Project\Keil\Reference_Local\STM32F103VET6_BaseFrame"
SKIP_DIRS = {'OBJ', '.vscode', 'RTE'}
SKIP_FILES = {
    'stm32f10x.h', 'system_stm32f10x.h', 'startup_stm32f10x_hd.s',
    'FONT.H', 'oledfont.h',
}

def should_skip(filepath):
    rel = os.path.relpath(filepath, BASE_DIR)
    for p in rel.split(os.sep)[:-1]:
        if p in SKIP_DIRS:
            return True
    return os.path.basename(filepath) in SKIP_FILES

def is_comment_line_to_delete(line):
    """判断一行注释是否应该被删除"""
    stripped = line.strip()
    
    if not stripped.startswith('//'):
        return False
    
    # 1. 纯分隔线
    if re.match(r'^//[/\*#=_\-]{5,}\s*$', stripped):
        return True
    if re.match(r'^[/\*#=_\-]{5,}\s*$', stripped):
        return True
    
    # 2. 版权信息
    copyright_keywords = [
        r'本程序只供学习使用',
        r'本程序仅供学习使用',
        r'未经作者许可',
        r'不得用于其它任何用途',
        r'不得用于商业目的',
        r'ALIENTEK',
        r'正点原子@',
        r'技术论坛.*openedv',
        r'版权所有，盗版必究',
        r'Copyright',
        r'All\s+[Rr]ights\s+[Rr]eserved',
        r'广州市星翼电子科技',
        r'仅限于学习使用',
        r'仅供学习参考',
        r'仅供学习',
    ]
    for pat in copyright_keywords:
        if re.search(pat, stripped):
            return True
    
    # 3. 版本历史 / 修改说明
    # 如: //V1.4修改说明, //V1.5 20120322, //V1.6 20120412
    if re.match(r'^//V\d+[\.\d]*\s', stripped):
        return True
    # 如: //1,增加MSR_MSP函数   (版本修改详情)
    if re.match(r'^//\d+[,，]', stripped):
        return True
    
    # 4. 描述型的单行头注释（非功能性说明）
    if re.match(r'^//[=]{2,}\s*$', stripped):
        return True
    
    return False

def clean_file(filepath):
    if should_skip(filepath):
        return False
    
    rel = os.path.relpath(filepath, BASE_DIR)
    
    with open(filepath, 'rb') as f:
        raw = f.read()
    
    # 尝试解码
    for enc in ('gbk', 'gb2312', 'utf-8', 'latin-1'):
        try:
            content = raw.decode(enc)
            break
        except:
            continue
    
    lines = content.splitlines(keepends=True)
    new_lines = []
    deleted_count = 0
    in_function_comment = False
    
    for i, line in enumerate(lines):
        stripped = line.strip()
        
        if is_comment_line_to_delete(line):
            deleted_count += 1
            continue
        
        # 保持空行合并（多个连续空行缩为1个），但保留换行结构
        new_lines.append(line)
    
    new_content = ''.join(new_lines)
    
    # 去掉文件末尾多余空行（最多保留1个）
    new_content = new_content.rstrip('\n\r') + '\n'
    
    # 去掉头部多余空行
    new_content = new_content.lstrip('\n\r')
    
    if new_content == content:
        return False
    
    # 转 GB2312
    try:
        new_bytes = new_content.encode('gb2312')
    except:
        new_bytes = new_content.encode('gbk', errors='replace')
    
    with open(filepath, 'wb') as f:
        f.write(new_bytes)
    
    print(f"  DONE: {rel} (deleted {deleted_count} lines)")
    return True

def main():
    print("=" * 60)
    print("批量处理源文件：删除非必要注释 + 转 GB2312")
    print("=" * 60)
    
    # 先删除 .bak 备份（之前运行的）
    for root, dirs, files in os.walk(BASE_DIR):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in files:
            if f.endswith('.bak') and f[:-4].endswith(('.c', '.h')):
                os.remove(os.path.join(root, f))
    
    all_files = []
    for root, dirs, files in os.walk(BASE_DIR):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in files:
            if f.endswith(('.c', '.h')):
                all_files.append(os.path.join(root, f))
    
    all_files.sort()
    
    count = 0
    for fp in all_files:
        if clean_file(fp):
            count += 1
    
    print(f"\n处理完成：{count} 个文件被修改")
    print("=" * 60)

if __name__ == '__main__':
    main()

