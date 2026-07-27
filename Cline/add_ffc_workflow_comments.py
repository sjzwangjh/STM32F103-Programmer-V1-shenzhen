#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
添加ff.c中函数内部的工作流程中文注释
先备份，再逐函数添加，保持编码GB2312，末尾2空白行
"""
import os
import shutil

ROOT = r'e:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1'
FP = os.path.join(ROOT, 'HARDWARE', 'FAT32', 'ff.c')
BACKUP = FP + '.bak'

# 备份原始文件
shutil.copy2(FP, BACKUP)

# 为每个函数函数体开头添加工作流注释
# 这些注释插入到每个函数的 '{' 之后
WORKFLOW_COMMENTS = {
    'disk_initialize': None,  # 不在ff.c中
    
    'move_window': None,  # 已完成
    
    'get_fat': (
        '    /* 工作流程：根据文件系统类型(FAT12/16/32)计算FAT表项所在的扇区 */\n'
        '    /* FAT32: 每扇区128个表项(4字节/项); FAT16: 每扇区256个(2字节/项) */\n'
        '    /* FAT12: 每扇区341个表项(1.5字节/项), 需要处理跨字节边界 */\n'
    ),
    
    'put_fat': (
        '    /* 工作流程：计算FAT表项所在扇区，加载后修改对应位置的值 */\n'
        '    /* 完成后设置wflag脏标记，等待move_window刷写回磁盘 */\n'
    ),
    
    'create_name': (
        '    /* 工作流程：从路径中提取最末段作为文件名，去除路径分隔符 */\n'
        '    /* 将文件名拆分为8字节主名和3字节扩展名，字母转为大写 */\n'
        '    /* 检查每个字符的合法性（仅允许字母/数字/特定符号） */\n'
    ),
    
    'get_sfn': (
        '    /* 工作流程：从11字节的8.3目录项中解析出可读文件名 */\n'
        '    /* 主名部分去尾部空格，扩展名前加"."，组合为完整文件名 */\n'
    ),
    
    'dir_sdi': (
        '    /* 工作流程：按索引号遍历目录，找到第index个目录项 */\n'
        '    /* FAT32根目录以簇链形式存储；FAT12/16根目录固定区域 */\n'
        '    /* 遍历每个目录簇中的所有扇区，扫描每个扇区中的目录项 */\n'
    ),
    
    'dir_find': (
        '    /* 工作流程：按索引枚举每个目录项，与目标8.3文件名比较 */\n'
        '    /* 跳过已删除项(0xE5)和长文件名项(ATTR_LFN) */\n'
        '    /* 遇到空项(0x00)表示后续无更多目录项，返回FR_NO_FILE */\n'
    ),
    
    'dir_alloc': (
        '    /* 工作流程：在目录中查找第一个空闲目录项位置 */\n'
        '    /* 空闲项包括：已删除项(首字节0xE5)或从未使用的空项(0x00) */\n'
    ),
    
    'create_chain': (
        '    /* 工作流程：遍历FAT表查找空闲簇(值为0)，标记为EOF后返回 */\n'
        '    /* 用于文件写入时分配新的数据簇 */\n'
    ),
    
    'remove_chain': (
        '    /* 工作流程：从起始簇开始沿FAT链遍历，逐个释放簇(写0) */\n'
        '    /* 直到遇到EOF标记或回到自身(循环检测) */\n'
    ),
    
    'get_fileinfo': (
        '    /* 工作流程：从32字节目录项中提取文件属性/大小/短文件名 */\n'
        '    /* 属性在偏移11，大小在偏移28(4字节)，文件名由get_sfn解析 */\n'
    ),
    
    'f_mount': (
        '    /* 工作流程：读取MBR/DBR扇区，解析BPB参数 */\n'
        '    /* 验证引导标志0x55AA；确定FAT类型(FAT32/16) */\n'
        '    /* 计算FAT表位置、数据区位置、根目录位置等关键参数 */\n'
    ),
    
    'f_open': (
        '    /* 工作流程：生成8.3短文件名→在目录中查找 */\n'
        '    /* 找到：根据mode处理(截断/返回已存在/打开) */\n'
        '    /* 未找到：若mode允许创建，在目录中分配新目录项 */\n'
        '    /* 最后：填充FIL文件对象并返回 */\n'
    ),
    
    'f_read': (
        '    /* 工作流程：按当前读写指针位置读取文件数据 */\n'
        '    /* 逐簇跟踪FAT链，逐扇区读取到用户缓冲区 */\n'
        '    /* 处理扇区内的偏移对齐，不超过文件大小 */\n'
    ),
    
    'f_write': (
        '    /* 工作流程：按当前读写指针位置写入数据 */\n'
        '    /* 遇到簇尾时自动分配新簇(create_chain)并链接到FAT链 */\n'
        '    /* 若文件起始簇为空，在目录项中记录新簇号 */\n'
        '    /* 更新文件大小(fsize)和脏标志(wflag) */\n'
    ),
    
    'f_sync': (
        '    /* 工作流程：先更新目录项中的文件大小和起始簇信息 */\n'
        '    /* 然后刷写窗口脏缓冲区到磁盘，最后执行磁盘同步 */\n'
    ),
    
    'f_close': (
        '    /* 工作流程：先调用f_sync同步缓存，再清空文件对象 */\n'
    ),
    
    'f_lseek': (
        '    /* 工作流程：限制偏移不超过文件大小，重置簇缓存 */\n'
    ),
    
    'f_opendir': (
        '    /* 工作流程：验证文件系统已挂载，初始化DIR对象 */\n'
        '    /* 根目录起始簇从已挂载的FatFs->dirbase获取 */\n'
    ),
    
    'f_readdir': (
        '    /* 工作流程：按当前索引遍历根目录 */\n'
        '    /* 跳过已删除项和长文件名项，遇到空项表示枚举结束 */\n'
        '    /* 找到有效项时用get_fileinfo提取信息返回 */\n'
    ),
    
    'f_stat': (
        '    /* 工作流程：生成短文件名→在目录中查找→提取文件信息返回 */\n'
    ),
    
    'f_truncate': (
        '    /* 工作流程：释放文件的所有数据簇→清空起始簇信息→同步 */\n'
    ),
    
    'f_unlink': (
        '    /* 工作流程：在目录中找到文件→释放数据簇→标记目录项为已删除(0xE5) */\n'
        '    /* 最后同步磁盘确保删除操作持久化 */\n'
    ),
    
    'f_rename': (
        '    /* 工作流程：生成新旧短文件名→检查新名是否已存在 */\n'
        '    /* 找到旧文件→将其目录项中的文件名改为新名→同步磁盘 */\n'
    ),
}

with open(FP, 'rb') as f:
    raw = f.read()
text = raw.decode('gb2312')

# 对于每个函数，查找函数定义后的 '{' 并插入工作流注释
insertions = []
lines = text.split('\r\n')

for func_name, comment in WORKFLOW_COMMENTS.items():
    if comment is None:
        continue
    
    # 查找函数名在行中
    found_at = []
    for idx, line in enumerate(lines):
        stripped = line.strip()
        # 匹配 "FUNC_NAME(...)" 或 "* FUNC_NAME -"
        if func_name in stripped and (stripped.endswith('(') or stripped.endswith('{') or func_name in stripped):
            found_at.append(idx)
    
    for idx in found_at:
        # 确认这个行后面有 '{'
        # Search forward from this line to find '{'
        brace_idx = -1
        for j in range(idx, min(idx + 5, len(lines))):
            if '{' in lines[j]:
                brace_idx = j
                break
        
        if brace_idx >= 0:
            # 找到 '{' 的行，在它之后插入
            # 检查是否已有工作流注释
            already_has = False
            for j in range(brace_idx + 1, min(brace_idx + 5, len(lines))):
                l = lines[j].strip()
                if '工作流程' in l or '工作流' in l:
                    already_has = True
                    break
            
            if not already_has:
                # 在brace_idx后面插入注释
                insertions.append((brace_idx, comment))

# 从后往前排序
insertions.sort(key=lambda x: x[0], reverse=True)

for idx, comment in insertions:
    # 找到 '{' 行的确切位置
    line = lines[idx]
    brace_pos = line.index('{')
    if brace_pos >= 0:
        # 在 '{' 之后、换行之前插入
        indent = ' ' * (len(line) - len(line.lstrip()))
        if line.rstrip().endswith('{'):
            lines[idx] = line.rstrip()
            comment_lines = comment.split('\n')
            for c in comment_lines:
                if c.strip():
                    lines.insert(idx + 1, indent + c.strip())
            lines.insert(idx + 1, '')

text = '\r\n'.join(lines)

try:
    new_raw = text.encode('gb2312')
except:
    new_raw = text.encode('gbk')

# 确保末尾2行空白行
if not new_raw.endswith(b'\r\n\r\n'):
    new_raw = new_raw.rstrip(b'\r\n') + b'\r\n\r\n'

with open(FP, 'wb') as f:
    f.write(new_raw)

print(f'Done! Processed {len(insertions)} functions')