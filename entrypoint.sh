#!/usr/bin/env bash

# Ensure the script exits on errors
set -e

# Default Docker working directory
WORK_DIR=${WORK_DIR:-/mdb/data}

# 设置权限
chmod 700 "$WORK_DIR"/*.dat

# Execute the container's main command
exec "$@"
