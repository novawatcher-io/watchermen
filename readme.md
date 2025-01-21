## 设计思路

watchermen作为一个进程管理器，主要职责如下：

- 管理一个或多个子程序，包括子程序的运行、停止、状态检查、配置文件更新并重新加载
- 与服务端保持连接，并接受纳管
- 使用CGROUP限制子进程的内存和CPU
- 上报服务运行指标
- 负责agent包自动更新升级

为了管理子程序，watchermen需要以父进程的身份运行子进程，并监视子进程，获取子进程的标准输出和标准错误，以便随时在需要的时候将信息上报给服务端。为此，我们定义这种子程序为`监视进程`。监视进程如果意外终止，watchermen会自动重启它。


## 开发环境

编程语言：

```
c++ 17
```

支持操作系统

```
linux
```

开发环境为ubuntu22.04

## 启动参数

- `-c`：指定合法的watchermen的配置文件路径


## 配置

watchermen的样例配置文件如下

```
syntax = "proto3";

message NetworkConfig {
  // 主机支持多个配置
  string host = 1;
  uint32 port = 2;
}

// CGroup 配置，限制CPU内存
message CGroupConfig {
  float memory = 1;
  float cpu = 2;
  bool enabled = 3;
  string name = 4;
}

message ProcessConfig {
  // 进程名字
  string process_name = 1;
  // 启动命令
  string command = 2;
  // 挂掉后是否自动重启
  bool autostart = 3;
  // 启动用户
  string user = 4;
  // 启动进程数
  uint32 numprocs = 5;
  // 停止信号
  int64  stopsignal = 6;
  // 停止信号发出后默认等待多久，超时后直接kill
  uint64 stopwaitsecs= 7;
  // 是否停止整个进程组
  bool stopasgroup = 8;
  // 标准错误输出是否重定向
  bool redirect_stderr = 9;
  // 标注输出位置
  string  stdout_logfile = 10;
  // 是否启动
  bool enabeld = 11;
  // CGroup
  CGroupConfig cgroup = 12;
}

message HttpHealthConfig {
  string path = 1;
}

message HttpMetricConfig {
  string path = 1;
}

message HttpServerConfig {
  // 主机支持多个配置
  string host = 1;
  uint32 port = 2;
  HttpHealthConfig healthConfig = 3;
  HttpMetricConfig httpMetricConfig = 4;
}

message ReloadConfig {
  // 主机支持多个配置
  uint32 timeout = 5;
}

message ManagerConfig {
  NetworkConfig network = 1;
  repeated ProcessConfig service = 2;
  string loglevel = 3;
  // CGroup
  CGroupConfig cgroup = 4;
  string cgroups_hierarchy = 5;
  HttpServerConfig httpServer = 6;
  ReloadConfig reload = 7;
}

```

### NetworkConfig
暂未开发
```
message NetworkConfig {
  // 主机支持多个配置
  string host = 1;
  uint32 port = 2;
}
```

服务器的ip和端口，用来上报watchermen健康检查状态、CPU、内存数据、agent安装包更新。

### CGroupConfig

```
// CGroup 配置，限制CPU内存
message CGroupConfig {
  float memory = 1;
  float cpu = 2;
  bool enabled = 3;
  string name = 4;
}
```

- memory agent最多使用内存，如果超过内存限制，则会触发内核OOM杀死进程
- cpu 子进程最大使用的cpu限额
- enabled 是否开启cgroup配置
- name cgroup 的名字，如果不设置，父层级为watchermen；子层级为 process_name

### ProcessConfig

```
message ProcessConfig {
  // 进程名字
  string process_name = 1;
  // 启动命令
  string command = 2;
  // 挂掉后是否自动重启
  bool autostart = 3;
  // 启动用户
  string user = 4;
  // 启动进程数
  uint32 numprocs = 5;
  // 停止信号
  int64  stopsignal = 6;
  // 停止信号发出后默认等待多久，超时后直接kill
  uint64 stopwaitsecs= 7;
  // 是否停止整个进程组
  bool stopasgroup = 8;
  // 标准错误输出是否重定向
  bool redirect_stderr = 9;
  // 标注输出位置
  string  stdout_logfile = 10;
  // 是否启动
  bool enabeld = 11;
  // CGroup
  CGroupConfig cgroup = 12;
}
```

以下为未实现功能

- autostart 废弃
- numprocs 未实现
- stopsignal 未实现
- stopwaitsecs 未实现
- stopasgroup 未实现
- redirect_stderr 未实现
- stdout_logfile 未实现
- stdout_logfile 未实现
- enabled 是否启动

### HttpServerConfig

http 服务 ip 端口

```
// 监控检查
message HttpHealthConfig {
  string path = 1;
}

//指标接口
message HttpMetricConfig {
  string path = 1;
}

```

- HttpMetricConfig 未实现

### ReloadConfig

```
message ReloadConfig {
  // 主机支持多个配置
  uint32 timeout = 5;
}
```

配置文件发生变动后5s后重启对应模块

- CGroupConfig,如果CGROUP父级发生变化，重启所有的process，子集发生变化，重启对应的process
- HttpServerConfig 发生变化，重启http模块
- ProcessConfig 发生变化，重新reload 对应的process

## 依赖第三方库清单


### 安装libcgroup

```
git submodule add git@github.com:libcgroup/libcgroup.git thrid_party/libcgroup
```

### 安装openssl

```
git submodule add git@github.com:openssl/openssl.git thrid_party/openssl
```

使用分支是openssl-3.2.1

### 安装nlohmann
```
git submodule add git@github.com:nlohmann/json.git thrid_party/nlohmann
```

### 安装fmt库

```
git submodule add git@github.com:fmtlib/fmt.git thrid_party/fmt
```

使用分支是10.2.1

### 安装jemalloc

```
git submodule add git@github.com:jemalloc/jemalloc.git thrid_party/jemalloc
```

### 安装libevent库

```
git submodule add git@github.com:libevent/libevent.git thrid_party/libevent
```

使用分支 v1.61.0

### 加入prometheus

```
git submodule add git@github.com:jupp0r/prometheus-cpp.git thrid_party/prometheus-cpp
```

使用分支prometheus-cpp

### protobuf 

由于protobuf编译机各个主机的版本可能不一致，所以统一用grpc的

### 加入grpc

```
git submodule add git@github.com:grpc/grpc.git thrid_party/grpc
```

使用分支grpc release-2.1.12-stable

## 使用`vcpkg`来管理第三方依赖

1. 安装vcpkg
```bash
git clone https://github.com/microsoft/vcpkg.git
export VCPKG_ROOT=$PWD/vcpkg
cd vcpkg
./bootstrap-vcpkg.sh
```

2. 安装依赖
```bash
cd watchmen
$VCPKG_ROOT/vcpkg install
```

3. 使用vcpkg工具链(推荐方式)
* 可以使用简单的方式, 通过`CMakePresets.json`
```bash
# 注意, default是`CMakePresets.json`里面的一个配置名
# 需要配置正确的`CMAKE_TOOLCHAIN_FILE`路径
cmake --preset default
```

* 或者通过设置变量`CMAKE_TOOLCHAIN_FILE`来使用vcpkg的工具链
```bash
cd watchermen
export CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake -S . -B build-with-vcpkg -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build-with-vcpkg
```
