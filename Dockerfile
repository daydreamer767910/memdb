ARG UBUNTU_VERSION=22.04
ARG TZ=Etc/UTC

FROM ubuntu:$UBUNTU_VERSION AS skeleton

# 设置环境变量
#ENV TZ=$TZ
ENV DEBIAN_FRONTEND=noninteractive

# 创建必要的目录结构
RUN mkdir -pv \
        /mdb/bin \
        /mdb/etc \
        /mdb/lib \
        /mdb/log \
        /mdb/data \
        /mdb/src \
        /mdb/build

# 配置时区
RUN apt-get update && apt-get install -y --no-install-recommends tzdata && \
    ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && \
    echo $TZ > /etc/timezone && \
    dpkg-reconfigure --frontend noninteractive tzdata && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /mdb

# This target builds the docker image
# This target can be useful to inspect the explicit outputs from the build,
FROM skeleton AS build

# 安装 C++ 编译工具和 CMake
RUN apt-get update && apt-get install -y \
    build-essential cmake libuv1-dev nlohmann-json3-dev pkg-config && \
    rm -rf /var/lib/apt/lists/*

# 复制项目源代码
COPY src /mdb/src

COPY etc /mdb/etc
COPY data /mdb/data

# 设置工作目录
WORKDIR /mdb/build

# 构建 C++ 库
RUN cmake ../src && make && make install


#############################
# Base runtime for services #
#############################

FROM skeleton AS runtime

ARG USER_ID=1000
ARG GROUP_ID=1000
ARG DOCKER_USER=mdb

# 复制 build 阶段的 libuv 和其他库文件
COPY --from=build /usr/lib/x86_64-linux-gnu/libuv* /usr/lib/x86_64-linux-gnu
COPY --from=build /mdb/etc/ /mdb/etc
COPY --from=build /mdb/data/ /mdb/data

VOLUME /mdb/etc

ENV PATH="/mdb/bin:$PATH"

RUN groupadd --gid "$GROUP_ID"  "$DOCKER_USER" && \
    useradd -d /mdb --uid "$USER_ID"  --gid "$GROUP_ID"  "$DOCKER_USER" && \
    passwd -d "$DOCKER_USER" && \
    chown -R "$DOCKER_USER:$DOCKER_USER" /mdb

COPY --chown=$USER_ID:$GROUP_ID \
	--chmod=755 \
	entrypoint.sh /mdb/entrypoint.sh

USER $DOCKER_USER

ENTRYPOINT ["/usr/bin/env", "bash", "/mdb/entrypoint.sh"]

###############
#  Mdb Server #
###############

FROM runtime AS mdbservice
LABEL description="Mdb Server"


COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=build \
     /mdb/bin/mdbsrv /mdb/bin/mdbsrv

COPY --chown=$DOCKER_USER:$DOCKER_USER \
     --from=build \
     /mdb/lib/ /mdb/lib

CMD ["mdbsrv"]