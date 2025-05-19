import os
import subprocess
import sys


def apply_patches(patch_dir):
    if not os.path.isdir(patch_dir):
        print(f"错误：指定的路径 '{patch_dir}' 不是一个有效的文件夹。")
        return

# 获取所有 .patch 文件并按文件名排序
    patches = sorted(f for f in os.listdir(patch_dir) if f.endswith('.patch'))

    if not patches:
        print("未找到任何 .patch 文件。")
        return

    print(f"发现 {len(patches)} 个补丁，开始依次应用...")

    for patch in patches:
        patch_path = os.path.join(patch_dir, patch)
        print(f"正在应用补丁：{patch}")
        try:
            subprocess.run(['git', 'apply', patch_path], check=True)
        except subprocess.CalledProcessError:
            print(f"应用补丁 {patch} 失败。请手动处理冲突。")
            break

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("用法：python apply_patches.py <patch目录路径>")
    else:
        patch_directory = sys.argv[1]
        apply_patches(patch_directory)

