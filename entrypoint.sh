#!/usr/bin/env bash

# Ensure the script exits on errors
set -e

# 检查 .env 文件是否存在
if [ -f ".env" ]; then
  # 加载 .env 文件中的变量
  export $(cat .env | grep -v '#' | xargs)
fi

# 监听 SIGTERM 和 SIGINT，确保进程被正确终止
_int() {
    echo "Caught SIGINT, stopping mdbsrv..."
    kill -INT "$child" 2>/dev/null || true
    wait "$child" 2>/dev/null || true  # 忽略退出码
    echo "mdbsrv stopped. Restarting..."
    # 重启 mdbsrv
    sleep 2  # 等待 2 秒后重启
}

# 监听信号
trap _int SIGTERM SIGINT

# 启动 mdbsrv（如果 `mdbsrv` 是 `$1`，则替换 `$1`）
if [ "$1" == "mdbsrv" ]; then
    shift  # 移除参数 `mdbsrv`
    # **自动重启逻辑**
    while true; do
        echo "Starting mdbsrv..."
        mdbsrv "$@" & # 启动进程，后台运行
        child=$!
        echo "mdbsrv started with PID $child"
        wait "$child" 2>/dev/null || true  # 忽略退出码
        exit_code=$?

        if [[ $exit_code -eq 0 ]]; then
            echo "mdbsrv exited normally, restarting..."
            #break  # 正常退出，则不重启
        else
            echo "mdbsrv crashed with code $exit_code, restarting..."
            sleep 2  # 等待 2 秒后重启
        fi
    done
else
    # 如果 `entrypoint.sh` 运行的是其他命令，直接 `exec`
    exec "$@"
fi
