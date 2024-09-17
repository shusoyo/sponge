#!/bin/bash

# 要格式化的目录
directories=("apps" "libsponge")

# 指定要格式化的文件扩展名
file_extensions=("hh" "cc")

# 检查 clang-format 是否存在
if ! command -v clang-format &> /dev/null
then
    echo "clang-format 未安装，请先安装 clang-format"
    exit 1
fi

# 遍历指定目录并格式化文件
for dir in "${directories[@]}"; do
    if [ -d "$dir" ]; then
        for ext in "${file_extensions[@]}"; do
            find "$dir" -type f -name "*.$ext" | while read -r file; do
                echo "格式化 $file ..."
                clang-format -i "$file"
            done
        done
    else
        echo "目录 $dir 不存在，跳过..."
    fi
done

echo "格式化完成！"
