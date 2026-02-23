import os
import sys
import re

# ---------------- 配置区域 ----------------
# 你的播放器中定义的模式列表（顺序必须与代码中一致！）
MODES = [
    "/儿歌",  # 对应 ID 0
    "/古诗",  # 对应 ID 1
    "/故事",  # 对应 ID 2
    "/音乐"   # 对应 ID 3
]

# 支持的音频格式
AUDIO_EXTS = {'.mp3', '.aac', '.flac', '.ogg', '.wav'}

# 要忽略的文件/文件夹（以 . 开头的隐藏文件默认忽略）
IGNORE_NAMES = {'System Volume Information', '$RECYCLE.BIN', '.Trashes', '.fseventsd'}
# ----------------------------------------

def is_audio_file(filename):
    _, ext = os.path.splitext(filename)
    return ext.lower() in AUDIO_EXTS

def clean_directory(root_path):
    """
    清理目录下的非音频文件
    """
    deleted_count = 0
    for root, dirs, files in os.walk(root_path):
        # 过滤隐藏目录
        dirs[:] = [d for d in dirs if not d.startswith('.') and d not in IGNORE_NAMES]
        
        for file in files:
            # 始终跳过 .playlist_cache_ 开头的文件（以防误删根目录下的缓存）
            if file.startswith('.playlist_cache_'):
                continue
                
            # 获取绝对路径
            file_path = os.path.join(root, file)
            _, ext = os.path.splitext(file)
            
            # 如果是隐藏文件(._开头) 或者 后缀不在白名单中
            if file.startswith('._') or (ext.lower() not in AUDIO_EXTS and not file.startswith('.')):
                try:
                    os.remove(file_path)
                    print(f"🗑️ 已删除: {file}")
                    deleted_count += 1
                except Exception as e:
                    print(f"❌ 删除失败 {file}: {e}")
                    
    if deleted_count > 0:
        print(f"🧹 共清理 {deleted_count} 个非音频文件")

def remove_duplicates(root_path):
    """
    删除重复的文件（例如：如果有 'song.mp3' 和 'song_1.mp3'，则删除 'song_1.mp3'）
    """
    deleted_count = 0
    # 匹配模式：文件名_数字.后缀 (例如: music_1.mp3)
    # group(1): 原文件名
    # group(2): 数字
    # group(3): 后缀
    pattern = re.compile(r'(.+)_(\d+)(\.\w+)$')
    
    for root, dirs, files in os.walk(root_path):
        # 过滤隐藏目录
        dirs[:] = [d for d in dirs if not d.startswith('.') and d not in IGNORE_NAMES]
        
        # 将当前目录下的所有文件放入集合，方便快速查找
        existing_files = set(files)
        
        for file in files:
            if file.startswith('.'): continue
            
            match = pattern.match(file)
            if match:
                base_name = match.group(1)
                ext = match.group(3)
                original_file = base_name + ext
                
                # 如果存在原文件（不带_N的），则认为当前文件是副本
                if original_file in existing_files:
                    file_path = os.path.join(root, file)
                    try:
                        os.remove(file_path)
                        print(f"✂️  已删除副本: {file} (原文件: {original_file})")
                        deleted_count += 1
                    except Exception as e:
                        print(f"❌ 删除失败 {file}: {e}")
    
    if deleted_count > 0:
        print(f"🧹 共清理 {deleted_count} 个重复副本文件")

def scan_directory(root_dir, mode_path):
    """
    扫描指定模式目录下的所有音频文件
    root_dir: SD卡根目录在电脑上的路径
    mode_path: 模式相对路径（如 "/儿歌"）
    """
    file_list = []
    
    # 拼接完整路径
    # 注意：Windows下路径可能带盘符，我们需要相对路径
    full_scan_path = os.path.join(root_dir, mode_path.lstrip('/'))
    
    if not os.path.exists(full_scan_path):
        print(f"⚠️ 警告: 目录不存在: {full_scan_path}")
        return []
    
    print(f"正在扫描: {mode_path} ...")
    
    # 执行清理
    clean_directory(full_scan_path)
    remove_duplicates(full_scan_path)
    
    for root, dirs, files in os.walk(full_scan_path):
        # 过滤隐藏目录
        dirs[:] = [d for d in dirs if not d.startswith('.') and d not in IGNORE_NAMES]
        
        for file in files:
            # 过滤隐藏文件
            if file.startswith('.'):
                continue
                
            if is_audio_file(file):
                # 获取绝对路径
                abs_path = os.path.join(root, file)
                # 转为相对于 SD 卡根目录的路径
                rel_path = os.path.relpath(abs_path, root_dir)
                # 强制转换为 Unix 风格路径 (/)
                unix_path = '/' + rel_path.replace(os.sep, '/')
                file_list.append(unix_path)
                
    return file_list

def main():
    if len(sys.argv) < 2:
        print("使用方法: python generate_playlist.py <SD卡路径>")
        print("示例: python generate_playlist.py /Volumes/SDCARD")
        print("示例(Win): python generate_playlist.py E:\\")
        sys.exit(1)
        
    sd_root = sys.argv[1]
    
    if not os.path.isdir(sd_root):
        print(f"错误: '{sd_root}' 不是一个有效的目录")
        sys.exit(1)
        
    print(f"SD卡根目录: {sd_root}")
    print("-" * 40)
    
    total_files = 0
    
    for idx, mode in enumerate(MODES):
        # 扫描该模式下的文件
        files = scan_directory(sd_root, mode)
        
        if files:
            # 生成缓存文件名: .playlist_cache_0.txt
            cache_filename = f".playlist_cache_{idx}.txt"
            cache_path = os.path.join(sd_root, cache_filename)
            
            try:
                with open(cache_path, 'w', encoding='utf-8') as f:
                    for line in files:
                        f.write(line + '\n')
                print(f"✅ 生成索引: {cache_filename} (包含 {len(files)} 首歌)")
                total_files += len(files)
            except Exception as e:
                print(f"❌ 写入失败 {cache_filename}: {e}")
        else:
            print(f"⚪ 模式 {mode} 为空，跳过生成")
            
    print("-" * 40)
    print(f"完成！共索引 {total_files} 首歌。")
    print("现在可以安全弹出 SD 卡并插入播放器了。")

if __name__ == "__main__":
    main()
