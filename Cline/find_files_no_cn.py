#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""查找所有 .c .h 文件中开头部分没有中文注释的文件"""
import os

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
SKIP_DIRS = {'OBJ', 'DebugConfig', 'RTE', '.git', '__pycache__'}
EXTS = {'.c', '.h'}

no_cn_files = []

for root, dirs, files in os.walk(PROJECT_ROOT):
    dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
    for f in sorted(files):
        ext = os.path.splitext(f)[1].lower()
        if ext not in EXTS:
            continue
        fp = os.path.join(root, f)
        with open(fp, 'rb') as h:
            try:
                data = h.read()
                text = data.decode('gb2312')
            except:
                continue
        
        lines = text.split('\r\n')
        
        # 检查前5行是否有中文
        has_cn = False
        for line in lines[:5]:
            if any(ord(c) > 127 for c in line):
                has_cn = True
                break
        
        rel = os.path.relpath(fp, PROJECT_ROOT)
        if not has_cn:
            # 显示文件内容摘要
            preview_lines = lines[:3]
            no_cn_files.append((rel, preview_lines))

if no_cn_files:
    print(f"找到 {len(no_cn_files)} 个文件开头无中文注释：")
    print()
    for rel, preview in no_cn_files:
        print(f"[{rel}]")
        for line in preview:
            print(f"  {line}")
        print()
else:
    print("所有文件开头都有中文注释！")