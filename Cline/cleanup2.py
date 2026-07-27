#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
第二轮清理：删除残留的版本历史、修改说明等注释
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

def should_delete(line, i, all_lines):
    """判断注释行是否应删除"""
    s = line.strip()
    if not s.startswith('//'):
        return False
    # 空注释行
    if s == '//':
        return True
    # 纯分隔线
    if re.match(r'^//[/\*#=_\-]{5,}$', s):
        return True
    # 版权信息
    if re.search(r'本程序只供|本程序仅供|未经作者|不得用于|ALIENTEK|正点原子|技术论坛|版权所有|盗版必究|Copyright|All\s+[Rr]ights|广州市星翼|仅限于学习', s):
        return True
    # Vx.x 版本行（含修改说明）
    if re.match(r'^//V\d+[\.\d]*', s):
        return True
    # 版本修改详情行（行首数字+逗号格式的行，前面是版本说明块）
    if re.match(r'^//\d+[,，]', s):
        return True
    # 检查上下文：如果在 Vx.x 版本说明块中（前一行或后一行含 Vx.x）
    if re.match(r'^//(把|加入|增加|修改|添加|增加|去掉|更新|变更|修复|优化|调整|删除|完善|支持|加入)', s):
        return True
    # 以 "//////" 结束的部分
    if re.match(r'^[/]{2,}$', s):
        return True
    return False

def clean_file(fp):
    if should_skip(fp):
        return False
    rel = os.path.relpath(fp, BASE_DIR)
    with open(fp, 'rb') as f:
        raw = f.read()
    for enc in ('gbk', 'gb2312', 'utf-8', 'latin-1'):
        try:
            content = raw.decode(enc)
            break
        except:
            continue
    lines = content.splitlines(keepends=True)
    new_lines = []
    del_count = 0
    for i, line in enumerate(lines):
        if should_delete(line, i, lines):
            del_count += 1
        else:
            new_lines.append(line)
    new_content = ''.join(new_lines).strip() + '\n'
    if new_content == content.strip() + '\n':
        return False
    try:
        new_bytes = new_content.encode('gb2312')
    except:
        new_bytes = new_content.encode('gbk', errors='replace')
    with open(fp, 'wb') as f:
        f.write(new_bytes)
    print(f"  DONE: {rel} (deleted {del_count} lines)")
    return True

def main():
    print("="*60)
    print("第二轮清理：删除版本历史等残留注释")
    print("="*60)
    files = []
    for r, ds, fs in os.walk(BASE_DIR):
        ds[:] = [d for d in ds if d not in SKIP_DIRS]
        for f in fs:
            if f.endswith(('.c','.h')):
                files.append(os.path.join(r, f))
    files.sort()
    cnt = 0
    for fp in files:
        if clean_file(fp):
            cnt += 1
    print(f"\n完成：{cnt} 个文件被清理")
    print("="*60)

if __name__ == '__main__':
    main()

