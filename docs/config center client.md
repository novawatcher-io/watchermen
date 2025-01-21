# 配置中心客户端行为

## 1. 配置中心客户端初始化
配置中心的地址在配置文件, 字段为`host`:
```json
{
    "network": {
        "host": "81.71.98.26",
        "port": 10050
    }
}
```

## 2. 连接后端的行为
### 主机离线的情况
* 如果主机离线, 配置中心客户端会尝试连接后端, 会无限重试;
* 重试的时间间隔从5s-10s(随机值)开始, 每次重试会增加5s-10s(随机值), 最大间隔为30s.

### 运行过程中断网
当任意接口返回连接失败时, 配置中心客户端会认为网络断开, 会进入上一节的重试机制.


## 测试用例

### 用防火墙屏蔽配置中心的地址

假定服务器的地址是`81.71.98.26`, 用防火墙屏蔽配置中心的地址:
```bash
# Block TCP connections to 81.71.98.26
sudo iptables -A OUTPUT -d 81.71.98.26 -p tcp -j REJECT

# Save the iptables rules (Debian/Ubuntu)
sudo sh -c "mkdir -p /etc/iptables && iptables-save > /etc/iptables/rules.v4"

# Verify the rule
sudo iptables -L -v -n --line-numbers
```

### 取消屏蔽配置中心的地址

假定配置中心的地址规则在`OUTPUT`链的第一条, 取消屏蔽配置中心的地址:

```bash
# List current iptables rules with line numbers
sudo iptables -L -v -n --line-numbers

# Delete the rule blocking IP 81.71.98.26 (assuming it's on line 3 of OUTPUT chain)
sudo iptables -D OUTPUT 1

# Save the iptables rules (Debian/Ubuntu)
sudo sh -c "mkdir -p /etc/iptables && iptables-save > /etc/iptables/rules.v4"

# Verify the rule has been deleted
sudo iptables -L -v -n --line-numbers
```