# 设计文档

## 概览配置文件

基础配置

```
syntax = "proto3";

message NetworkConfig {
  // 主机支持多个配置
  string host = 1;
  uint32 port = 2;
}

// CGroup 配置，限制CPU内存
message CGroupConfig {
  uint64 memory = 1;
  uint64 cpu = 2;
  bool enabled = 3;
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

message ManagerConfig {
    NetworkConfig network = 1;
    repeated ProcessConfig service = 2;
    string loglevel = 3;
  // CGroup
  CGroupConfig cgroup = 12;
  string cgroups_hierarchy = 13;
}
```