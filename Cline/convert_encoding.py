#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
遍历项目中的所有 .c, .h, .s 源文件：
1. 检查编码，如果不是 GB2312，则转换为 GB2312，且不许有乱码
2. 如果文件不是以空白行结尾，在文件最后插入两行空白行（"\r\n"）
"""
import os

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))

# 跳过的目录
SKIP_DIRS = {'OBJ', 'DebugConfig', 'RTE', '.git', '__pycache__', '.vscode'}

# 跳过的文件（非文本源码）
SKIP_FILES = set()

# 扩展名
TARGET_EXT = {'.c', '.h', '.s', '.cpp', '.hpp', '.txt', '.ini', '.uvprojx', '.uvoptx', '.uvguix', '.scvd', '.py', '.md'}

def get_all_source_files(root_dir):
    """获取所有需要处理的源文件"""
    files = []
    for root, dirs, filenames in os.walk(root_dir):
        # 跳过不处理的目录
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in filenames:
            ext = os.path.splitext(f)[1].lower()
            if ext in TARGET_EXT and f not in SKIP_FILES:
                files.append(os.path.join(root, f))
    return files

def detect_and_convert_encoding(raw_bytes):
    """
    检测并转换编码到GB2312。
    返回 (converted_bytes, was_converted, error_msg)
    """
    # GB2312 尝试
    try:
        raw_bytes.decode('gb2312')
        return raw_bytes, False, None  # 已经是GB2312
    except (UnicodeDecodeError, UnicodeEncodeError):
        pass
    except Exception as e:
        pass

    # 尝试其他编码，找到能正确解码的
    content = None
    used_enc = None
    
    for enc in ('utf-8-sig', 'utf-8', 'gbk', 'latin-1', 'cp1252', 'big5'):
        try:
            decoded = raw_bytes.decode(enc)
            # 检查是否有不可逆的字符
            content = decoded
            used_enc = enc
            break
        except (UnicodeDecodeError, UnicodeEncodeError):
            continue
        except:
            continue
    
    if content is None:
        # 最后尝试 latin-1 (永远不会失败，但可能有乱码)
        content = raw_bytes.decode('latin-1')
        used_enc = 'latin-1'
    
    # 尝试编码为 GB2312
    try:
        new_raw = content.encode('gb2312')
        return new_raw, True, None
    except (UnicodeEncodeError, UnicodeDecodeError):
        # 有字符无法映射到GB2312，尝试用GBK
        try:
            new_raw = content.encode('gbk')
            if used_enc != 'gbk':
                print(f"    编码 {used_enc} -> GBK (GB2312超集，部分字符用GBK)")
            return new_raw, True, 'used_gbk'
        except:
            # 仍然失败，替换无法编码的字符
            new_raw = content.encode('gbk', errors='replace')
            print(f"    警告：使用replace策略处理无法映射的字符 (原编码: {used_enc})")
            return new_raw, True, 'replaced'

def ensure_trailing_blank_lines(content_bytes):
    """
    确保文件以两个空行结束。
    空行定义为只包含 "\r\n" 的行。
    """
    # 检查最后两个完整行是否为空白行
    # 将内容按 \r\n 分割
    lines = content_bytes.split(b'\r\n')
    
    # 检查最后一行是否为空（文件末尾可能有空洞）
    trailing_empty = 0
    for i in range(len(lines) - 1, -1, -1):
        if lines[i] == b'':
            trailing_empty += 1
        else:
            break
    
    need_blank_lines = 2 - trailing_empty
    
    if need_blank_lines <= 0:
        return content_bytes, False
    
    # 追加需要的空白行
    result = content_bytes
    for _ in range(need_blank_lines):
        result += b'\r\n'
    
    return result, True

def process_file(filepath):
    """处理单个文件"""
    rel_path = os.path.relpath(filepath, PROJECT_ROOT)
    
    try:
        with open(filepath, 'rb') as f:
            raw = f.read()
    except Exception as e:
        return f"  ERROR 读取失败: {e}"
    
    if len(raw) == 0:
        return f"  SKIP (空文件)"
    
    # 1. 编码转换
    converted, was_converted, conv_msg = detect_and_convert_encoding(raw)
    
    # 2. 空白行处理
    final_content, blanks_added = ensure_trailing_blank_lines(converted)
    
    if not was_converted and not blanks_added:
        return None  # 无需处理
    
    # 写回文件
    try:
        with open(filepath, 'wb') as f:
            f.write(final_content)
    except Exception as e:
        return f"  ERROR 写入失败: {e}"
    
    msgs = []
    if was_converted:
        msgs.append("编码已转换")
    if blanks_added:
        msgs.append("已追加空白行")
    
    return f"  处理: {', '.join(msgs)}"

def main():
    print("=" * 70)
    print(f"项目根目录: {PROJECT_ROOT}")
    print("任务说明：")
    print("  1. 将所有源文件转换为 GB2312 编码")
    print("  2. 确保文件以两行空白行结尾")
    print("=" * 70)
    
    source_files = get_all_source_files(PROJECT_ROOT)
    print(f"\n找到 {len(source_files)} 个源文件需要检查\n")
    
    count_processed = 0
    count_total = 0
    
    for fp in source_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        count_total += 1
        result = process_file(fp)
        if result is not None:
            print(f"[{rel}]")
            print(result)
            count_processed += 1
        else:
            # 显示进度（每10个文件）
            if count_total % 10 == 0 or count_total == len(source_files):
                print(f"  检查中... ({count_total}/{len(source_files)}) 已处理 {count_processed} 个", end='\r')
    
    print(f"\n\n{'=' * 70}")
    print(f"处理完成！总计 {count_total} 个文件，修改了 {count_processed} 个文件")
    print(f"{'=' * 70}")

if __name__ == '__main__':
    main()

