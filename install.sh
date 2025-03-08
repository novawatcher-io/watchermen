#!/bin/bash
PROJECTDIR="$(pwd)"

# 切换到要使用的分支
cd $PROJECTDIR/third_party/prometheus-cpp && git checkout openssl-3.2.1 && cd $PROJECTDIR
cd $PROJECTDIR/third_party/grpc && git checkout v1.61.0 && cd $PROJECTDIR
cd $PROJECTDIR/third_party/libevent && git checkout release-2.1.12-stable && cd $PROJECTDIR
cd $PROJECTDIR/third_party/prometheus-cpp && git checkout v1.2.1 && cd $PROJECTDIR

git submodule update --init --recursive

# 安装第三方库
cd $PROJECTDIR/third_party/jemalloc && ./autogen.sh && make -j4 && sudo make install && cd $PROJECTDIR
cd $PROJECTDIR/third_party/openssl && ./config && make -j4 && make install && cd $PROJECTDIR
cd $PROJECTDIR/third_party/grpc && mkdir -p build && cd build && cmake .. && make -j4 && make install && cd $PROJECTDIR
cd $PROJECTDIR/third_party/libevent && mkdir -p build && cd build && cmake -DOPENSSL_ROOT_DIR=$PROJECTDIR/thrid_party/openssl/ .. && make -j4 && make install && cd $PROJECTDIR
cd $PROJECTDIR/third_party/prometheus-cpp && mkdir -p build && cd build && cmake -DENABLE_TESTING=OFF .. && make -j4 && make install && cd $PROJECTDIR