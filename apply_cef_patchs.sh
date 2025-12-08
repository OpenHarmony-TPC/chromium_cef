#!/bin/bash

# 脚本用法说明
usage() {
    echo "Usage: $0 <chromium_src_path> <cef_path>"
    echo "Example: $0 /path/to/chromium/src /path/to/cef"
    exit 1
}

# 参数检查
if [ $# -ne 2 ]; then
    usage
fi

CHROMIUM_SRC_PATH=$1
CEF_PATH=$2

# 1. 检查路径是否存在
if [ ! -d "$CHROMIUM_SRC_PATH" ] || [ ! -d "$CEF_PATH" ]; then
    echo "Error: Invalid path specified"
    usage
fi

# 2. 复制CEF到chromium/src目录
echo "Copying CEF to chromium/src..."
cp -r "$CEF_PATH" "$CHROMIUM_SRC_PATH/cef" || {
    echo "Error: Failed to copy CEF directory"
    exit 1
}

# 3. 拷贝图标文件到指定位置
echo "Copying icon files..."
TARGET_ICON_DIR="$CHROMIUM_SRC_PATH/ohos/app/ohos_hap/AppScope/resources/base/media"
mkdir -p "$TARGET_ICON_DIR" || {
    echo "Error: Failed to create target directory $TARGET_ICON_DIR"
    exit 1
}

ICON_FILES=("app_icon.png" "icon.png")
for icon in "${ICON_FILES[@]}"; do
    if [ -f "$CEF_PATH/resource/$icon" ]; then
        cp "$CEF_PATH/resource/$icon" "$TARGET_ICON_DIR/" || {
            echo "Error: Failed to copy $icon"
            exit 1
        }
    else
        echo "Warning: Icon file $icon not found in $CEF_PATH/resource/"
    fi
done

# 4. 处理patch文件
PATCHES_DIR="$CEF_PATH/ohos_patchs"

if [ ! -d "$PATCHES_DIR" ]; then
    echo "Error: ohos_patchs directory not found in $CEF_PATH"
    exit 1
fi

# 4.1 获取并按自然顺序排序patch文件
echo "Collecting patches in natural order..."
mapfile -t PATCH_FILES < <(find "$PATCHES_DIR" -name "*.patch" -printf "%f\n" | sort -V)

# 4.2 复制patch文件到chromium目录（保持原名）
echo "Copying patches..."
for patch in "${PATCH_FILES[@]}"; do
    cp "$PATCHES_DIR/$patch" "$CHROMIUM_SRC_PATH/" || {
        echo "Error: Failed to copy patch $patch"
        exit 1
    }
done

# 5. 按自然顺序应用所有patch
echo "Applying patches in natural order..."
cd "$CHROMIUM_SRC_PATH" || exit 1

for patch in "${PATCH_FILES[@]}"; do
    echo "Applying $patch..."
    if ! patch -p2 < "$patch"; then
        echo "Error: Failed to apply patch $patch"
        exit 1
    fi
done

# 6. 删除已应用的patch文件
echo "Cleaning up applied patches..."
for patch in "${PATCH_FILES[@]}"; do
    rm -f "$patch"
done

echo "All operations completed successfully!"
