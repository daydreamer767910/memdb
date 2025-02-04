#!/usr/bin/env bash

# Ensure the script exits on errors
set -e

# 检查 .env 文件是否存在
if [ -f ".env" ]; then
  # 加载 .env 文件中的变量
  export $(grep -v '^#' .env | xargs)
fi

_term() {
    echo "Caught SIGTERM, stopping mdbsrv..."
    kill -TERM "$child" 2>/dev/null || true
    wait "$child" 2>/dev/null || true
    echo "mdbsrv stopped. Exiting..."
    exit 0
}

_int() {
    echo "Caught SIGINT, stopping mdbsrv..."
    kill -INT "$child" 2>/dev/null || true
    wait "$child" 2>/dev/null || true
    echo "mdbsrv stopped. Restarting..."
    sleep 2
}

trap _term SIGTERM
trap _int SIGINT

if [ "$1" == "mdbsrv" ]; then
    shift
    while true; do
        echo "Starting mdbsrv..."
        mdbsrv "$@" &  # 以后台进程方式启动
        child=$!
        echo "mdbsrv started with PID $child"
        wait "$child"  # 等待子进程结束

        exit_code=$?
        if [[ $exit_code -eq 0 ]]; then
            echo "mdbsrv exited normally, restarting..."
        else
            echo "mdbsrv crashed with code $exit_code, restarting..."
        fi
        sleep 2  # 等待 2 秒再重启
    done
else
    exec "$@"
fi