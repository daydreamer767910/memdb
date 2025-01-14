#!/usr/bin/env bash

# Ensure the script exits on errors
set -e

# Default Docker working directory
WORK_DIR=${WORK_DIR:-/mdb/data}

# 设置权限
chmod 700 "$WORK_DIR"/*.dat

# 检查 .env 文件是否存在
if [ -f ".env" ]; then
  # 加载 .env 文件中的变量
  export $(cat .env | grep -v '#' | xargs)
fi

# Execute the container's main command
exec "$@"
