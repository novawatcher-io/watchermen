# 安装第三方库

## 添加第三方库

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