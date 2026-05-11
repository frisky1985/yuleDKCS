# BER-TLV 协议故障排查手册

> **版本**: v1.0.0  
> **协议版本**: v1.2.0  
> **适用场景**: 生产环境故障排查

---

## 1. 错误码速查表

### 1.1 协议级错误 (0x0000-0x00FF)

| 错误码 | 名称 | 描述 | 排查步骤 |
|--------|------|------|----------|
| 0x0000 | SUCCESS | 成功 | - |
| 0x0001 | INVALID_MESSAGE | 消息格式无效 | 1. 检查HEX前3字节是否为0x80 0x03 0x01  <br>2. 验证BER长度编码 |
| 0x0002 | UNSUPPORTED_VERSION | 不支持的版本 | 1. 检查0x80 TAG值 <br>2. 确认双方支持的版本列表 |
| 0x0003 | AUTHENTICATION_FAILED | 认证失败 | 1. 检查HMAC签名 <br>2. 验证密钥同步状态 |
| 0x0004 | AUTHORIZATION_FAILED | 授权失败 | 1. 检查用户权限 <br>2. 验证token有效期 |
| 0x0005 | RATE_LIMIT_EXCEEDED | 频率限制 | 1. 检查消息发送频率 <br>2. 查看限流配置 |
| 0x0006 | TIMEOUT | 超时 | 1. 检查网络延迟 <br>2. 验证超时配置 |
| 0x0007 | ENCRYPTION_FAILED | 加密失败 | 1. 检查密钥有效性 <br>2. 验证算法支持 |
| 0x0008 | COMPRESSION_FAILED | 压缩失败 | 1. 检查ZLIB/LZ4库版本 <br>2. 验证数据可压缩性 |

### 1.2 应用级错误 (0x0100-0x01FF)

| 错误码 | 名称 | 触发场景 | 排查命令 |
|--------|------|----------|----------|
| 0x0100 | INVALID_COMMAND | 未知命令类型 | `grep "command_type" /var/log/tlv.log` |
| 0x0101 | VEHICLE_OFFLINE | 车辆离线 | `mosquitto_sub -t "vehicle/+/status" -v` |
| 0x0102 | COMMAND_TIMEOUT | 命令执行超时 | `tail -f /var/log/command_executor.log` |
| 0x0103 | HARDWARE_ERROR | 硬件故障 | `dmesg \| grep -i error` |
| 0x0104 | NOT_IN_PARK | 车辆未挂P档 | 查看车辆状态上报 |

### 1.3 OTA错误 (0x0200-0x02FF)

| 错误码 | 名称 | 解决方案 |
|--------|------|----------|
| 0x0200 | OTA_FAILED | 查看OTA日志，检查签名验证 |
| 0x0201 | SIGNATURE_INVALID | 重新下载升级包 |
| 0x0202 | INSUFFICIENT_SPACE | 清理存储空间 |
| 0x0203 | BATTERY_TOO_LOW | 充电后再升级 |
| 0x0204 | VERSION_MISMATCH | 检查版本兼容性列表 |

---

## 2. 快速诊断脚本

### 2.1 消息完整性检查

```bash
#!/bin/bash
# check_tlv_integrity.sh - TLV消息完整性检查

TLV_HEX="$1"

if [ -z "$TLV_HEX" ]; then
    echo "Usage: $0 <hex_string>"
    echo "Example: $0 '80030102008101058210a1b2c3d4...'"
    exit 1
fi

# 移除空格和换行
HEX=$(echo "$TLV_HEX" | tr -d ' \n')

echo "=== TLV消息完整性检查 ==="
echo "输入长度: ${#HEX} 字符 (${#HEX}/2 字节)"

# 检查最小长度
if [ ${#HEX} -lt 8 ]; then
    echo "❌ 错误: 消息太短，至少4字节"
    exit 1
fi

# 解析第一个TAG (应该是0x80 - 协议版本)
FIRST_TAG="${HEX:0:2}"
echo ""
echo "1. 首TAG检查"
echo "   发现: 0x$FIRST_TAG"

if [ "$FIRST_TAG" != "80" ]; then
    echo "   ⚠️ 警告: 首TAG不是0x80，不符合规范"
else
    echo "   ✅ 通过: 首TAG是0x80 (协议版本)"
fi

# 解析版本号
VERSION_LEN="${HEX:2:2}"
if [ "$VERSION_LEN" = "03" ]; then
    MAJOR="${HEX:4:2}"
    MINOR="${HEX:6:2}"
    PATCH="${HEX:8:2}"
    echo ""
    echo "2. 协议版本"
    echo "   版本: $((16#$MAJOR)).$((16#$MINOR)).$((16#$PATCH))"
    
    if [ "$MAJOR" = "01" ] && [ "$MINOR" = "02" ]; then
        echo "   ✅ 通过: 支持v1.2.x"
    else
        echo "   ⚠️ 警告: 版本可能不兼容"
    fi
else
    echo "   ❌ 错误: 版本长度字段异常 (期望03，得到$VERSION_LEN)"
fi

# 检查必须TAG
echo ""
echo "3. 必须TAG检查"
for TAG in 80 81 82 83; do
    if echo "$HEX" | grep -q "$TAG"; then
        echo "   ✅ 0x$TAG 存在"
    else
        echo "   ❌ 0x$TAG 缺失"
    fi
done

# 统计TLV元素个数
echo ""
echo "4. 结构分析"
python3 << 'EOF'
import sys
hex_str = sys.argv[1]
data = bytes.fromhex(hex_str)
offset = 0
count = 0

while offset < len(data):
    if offset >= len(data):
        break
    tag = data[offset]
    offset += 1
    
    if offset >= len(data):
        print(f"   截断在TAG 0x{tag:02X}")
        break
    
    first = data[offset]
    offset += 1
    
    if first & 0x80 == 0:
        length = first
    else:
        num_bytes = first & 0x7F
        length = 0
        for _ in range(num_bytes):
            if offset >= len(data):
                print(f"   截断在长度字段")
                break
            length = (length << 8) | data[offset]
            offset += 1
    
    print(f"   元素{count+1}: TAG=0x{tag:02X}, 长度={length}")
    
    if offset + length > len(data):
        print(f"   ❌ 数据截断: 需要{length}字节，剩余{len(data)-offset}字节")
        break
    
    offset += length
    count += 1

print(f"   总计: {count} 个TLV元素")
EOF
$HEX

echo ""
echo "=== 检查完成 ==="
```

### 2.2 网络连通性检查

```bash
#!/bin/bash
# check_network.sh - 网络连通性诊断

echo "=== TLV协议网络诊断 ==="

# MQTT Broker连接检查
echo ""
echo "1. MQTT Broker连通性"
MQTT_HOST="${MQTT_HOST:-mqtt.automotive.com}"
MQTT_PORT="${MQTT_PORT:-8883}"

echo "   目标: $MQTT_HOST:$MQTT_PORT"

# TCP连通性
if timeout 5 bash -c "cat < /dev/null > /dev/tcp/$MQTT_HOST/$MQTT_PORT" 2>/dev/null; then
    echo "   ✅ TCP连通"
else
    echo "   ❌ TCP不通，检查防火墙/网络"
fi

# TLS证书检查
echo ""
echo "2. TLS证书检查"
echo "   Q" | openssl s_client -connect $MQTT_HOST:$MQTT_PORT -servername $MQTT_HOST 2>/dev/null | openssl x509 -noout -dates -subject 2>/dev/null | while read line; do
    echo "   $line"
done

# 测试订阅
echo ""
echo "3. MQTT订阅测试 (5秒)"
timeout 5 mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT -t "test/#" --cafile /etc/ssl/certs/ca-certificates.crt 2>&1 | head -5 || echo "   订阅超时或失败"

# 延迟测试
echo ""
echo "4. 网络延迟"
ping -c 3 $MQTT_HOST 2>/dev/null | tail -1 || echo "   ping不可达"

echo ""
echo "=== 诊断完成 ==="
```

### 2.3 日志分析脚本

```bash
#!/bin/bash
# analyze_tlv_logs.sh - TLV日志分析

LOG_FILE="${1:-/var/log/tlv.log}"
OUTPUT_DIR="${2:-./log_analysis}"

mkdir -p $OUTPUT_DIR

echo "=== TLV日志分析 ==="
echo "日志文件: $LOG_FILE"
echo ""

# 1. 错误统计
echo "1. 错误码分布"
grep -oE "error_code\":[[:space:]]*[0-9xA-Fa-f]+" "$LOG_FILE" | \
    sed 's/.*: //' | sort | uniq -c | sort -rn | head -20 > "$OUTPUT_DIR/error_codes.txt"
cat "$OUTPUT_DIR/error_codes.txt"

# 2. 消息类型统计
echo ""
echo "2. 消息类型分布"
grep -oE "message_type\":[[:space:]]*"[A-Z_]+"" "$LOG_FILE" | \
    sed 's/.*: "//; s/"$//' | sort | uniq -c | sort -rn > "$OUTPUT_DIR/message_types.txt"
cat "$OUTPUT_DIR/message_types.txt"

# 3. 延迟分析
echo ""
echo "3. 消息处理延迟 (ms)"
grep -oE "latency_ms\":[[:space:]]*[0-9]+" "$LOG_FILE" | \
    sed 's/.*: //' | awk '
    BEGIN {count=0; sum=0; max=0}
    {sum+=$1; count++; if($1>max) max=$1}
    END {
        if(count>0) {
            print "   样本数:", count
            print "   平均值:", sum/count
            print "   最大值:", max
        }
    }'

# 4. 版本不匹配事件
echo ""
echo "4. 版本不匹配事件"
grep -c "UNSUPPORTED_VERSION\|version.*mismatch" "$LOG_FILE" 2>/dev/null | \
    xargs -I {} echo "   次数: {}"

# 5. 连接中断统计
echo ""
echo "5. 连接中断原因"
grep -oE "disconnect_reason\":[[:space:]]*"[A-Z_]+"" "$LOG_FILE" | \
    sed 's/.*: "//; s/"$//' | sort | uniq -c | sort -rn | head -10

echo ""
echo "=== 分析完成 ==="
echo "详细报告: $OUTPUT_DIR/"
```

---

## 3. 常见问题排查流程

### 3.1 消息无法解码

```
症状: 收到错误码 0x0001 (INVALID_MESSAGE)

排查流程:
1. 获取原始HEX
   $ tcpdump -i eth0 -w capture.pcap port 8883
   $ tshark -r capture.pcap -T fields -e data | head -1

2. 验证消息完整性
   $ ./check_tlv_integrity.sh "<hex_string>"

3. 检查字节序
   - TLV使用大端序 (Big Endian)
   - 某些嵌入式平台默认小端序

4. 验证BER长度编码
   - 短格式: 长度 < 128, 单字节
   - 长格式: 长度 >= 128, 首字节 0x81/0x82/0x83

5. 检查加密/压缩
   - 确认双方加密配置一致
   - 验证压缩算法 (ZLIB/LZ4)
```

### 3.2 版本协商失败

```
症状: 收到错误码 0x0002 (UNSUPPORTED_VERSION)

排查步骤:
1. 查看支持的版本列表
   $ grep "supported_versions" /etc/tlv/config.yaml

2. 检查对端版本
   - 捕获VERSION_NEGOTIATION消息
   - 解析0x80 TAG后的3字节

3. 降级策略检查
   - 确认fallback版本配置
   - 检查蓝绿部署状态

4. 强制版本兼容 (紧急处理)
   # 临时允许v1.1.x
   echo "allowed_versions: [1.2.0, 1.1.0]" >> /etc/tlv/config.yaml
   systemctl restart tlv-gateway
```

### 3.3 车辆离线

```
症状: 错误码 0x0101 (VEHICLE_OFFLINE)

排查流程:
1. 检查车辆T-Box状态
   $ mosquitto_sub -t "vehicle/<vin>/status" -v
   # 期望: {"online": true, "last_seen": <timestamp>}

2. 信号强度检查
   $ curl http://vehicle-api/stats?vin=<vin> | jq '.signal_strength'
   # 正常值: > -100 dBm

3. MQTT连接状态
   $ emqx_ctl conn stats | grep <vin>

4. 固件版本检查
   $ curl http://vehicle-api/info?vin=<vin> | jq '.firmware_version'
   # 检查是否过旧

5. 远程诊断
   $ ssh tbox@<vehicle_ip> 'systemctl status tlv-client'
```

### 3.4 性能下降

```
症状: 消息延迟增加，吞吐量下降

排查清单:
□ CPU使用率
  $ top -p $(pgrep -d',' tlv)
  
□ 内存使用
  $ free -h
  $ cat /proc/$(pgrep tlv)/status | grep VmRSS

□ 网络带宽
  $ ifstat -i eth0 1 10

□ 消息队列堆积
  $ redis-cli LLEN tlv:queue:pending

□ 数据库慢查询
  $ tail -f /var/log/postgresql/slow.log

□ GC频率 (Java)
  $ jstat -gc $(pgrep -f tlv-gateway) 1000

优化命令:
# 查看热点方法
$ perf top -p $(pgrep tlv)

# 生成火焰图
$ perf record -F 99 -p $(pgrep tlv) -g -- sleep 30
$ perf script | ./stackcollapse-perf.pl | ./flamegraph.pl > tlv.svg
```

---

## 4. Wireshark插件使用

### 4.1 TLV协议解析器

```lua
-- tlv_dissector.lua - Wireshark TLV协议解析插件
-- 安装: 复制到 ~/.local/lib/wireshark/plugins/

local tlv_proto = Proto("TLV", "BER-TLV Protocol")

-- 定义字段
local f_tag = ProtoField.uint8("tlv.tag", "TAG", base.HEX)
local f_length = ProtoField.uint32("tlv.length", "Length", base.DEC)
local f_value = ProtoField.bytes("tlv.value", "Value")
local f_tag_name = ProtoField.string("tlv.tag_name", "Tag Name")

local f_version_major = ProtoField.uint8("tlv.version.major", "Major")
local f_version_minor = ProtoField.uint8("tlv.version.minor", "Minor")
local f_version_patch = ProtoField.uint8("tlv.version.patch", "Patch")

tlv_proto.fields = {f_tag, f_length, f_value, f_tag_name, 
                   f_version_major, f_version_minor, f_version_patch}

-- TAG名称映射
local tag_names = {
    [0x80] = "PROTOCOL_VERSION",
    [0x81] = "MESSAGE_TYPE",
    [0x82] = "MESSAGE_ID",
    [0x83] = "TIMESTAMP",
    [0x84] = "SESSION_ID",
    [0x01] = "BOOLEAN",
    [0x02] = "INTEGER",
    [0x0C] = "UTF8STRING"
}

-- 解析函数
function tlv_proto.dissector(buffer, pinfo, tree)
    local length = buffer:len()
    if length == 0 then return end
    
    pinfo.cols.protocol = tlv_proto.name
    
    local subtree = tree:add(tlv_proto, buffer(), "BER-TLV Protocol Data")
    local offset = 0
    local element_count = 0
    
    while offset < length do
        element_count = element_count + 1
        local elem_tree = subtree:add(buffer(offset, math.min(10, length-offset)), 
                                      "Element " .. element_count)
        
        -- 读取TAG
        local tag = buffer(offset, 1):uint()
        elem_tree:add(f_tag, buffer(offset, 1))
        elem_tree:add(f_tag_name, tag_names[tag] or "UNKNOWN")
        offset = offset + 1
        
        if offset >= length then break end
        
        -- 读取长度
        local first = buffer(offset, 1):uint()
        local len_size = 1
        local len_value = 0
        
        if bit.band(first, 0x80) == 0 then
            len_value = first
        else
            local num_bytes = bit.band(first, 0x7F)
            len_size = 1 + num_bytes
            for i = 1, num_bytes do
                len_value = bit.lshift(len_value, 8) + buffer(offset + i, 1):uint()
            end
        end
        
        elem_tree:add(f_length, len_value)
        offset = offset + len_size
        
        if offset + len_value > length then
            elem_tree:add_expert_info(PI_MALFORMED, PI_ERROR, "Truncated data")
            break
        end
        
        -- 读取值
        if len_value > 0 then
            elem_tree:add(f_value, buffer(offset, len_value))
            
            -- 特殊处理: 版本号
            if tag == 0x80 and len_value == 3 then
                elem_tree:add(f_version_major, buffer(offset, 1))
                elem_tree:add(f_version_minor, buffer(offset+1, 1))
                elem_tree:add(f_version_patch, buffer(offset+2, 1))
            end
        end
        
        offset = offset + len_value
    end
    
    subtree:append_text(", " .. element_count .. " elements")
end

-- 注册到TCP 8883端口 (MQTT over TLS)
local tcp_table = DissectorTable.get("tcp.port")
tcp_table:add(8883, tlv_proto)
```

### 4.2 使用示例

```bash
# 1. 安装插件
cp tlv_dissector.lua ~/.local/lib/wireshark/plugins/

# 2. 捕获流量
sudo tcpdump -i eth0 -w tlv_traffic.pcap port 8883

# 3. 使用Wireshark打开
wireshark tlv_traffic.pcap

# 4. 过滤TLV消息
# 显示过滤: tlv.tag == 0x80
# 显示特定消息类型: tlv.tag == 0x81 && tlv.value == 05
```

---

## 5. 监控告警配置

### 5.1 Prometheus告警规则

```yaml
# tlv_alerts.yml
groups:
  - name: tlv_protocol_alerts
    rules:
      # 解码错误率过高
      - alert: TLVHighDecodeErrorRate
        expr: |
          (
            sum(rate(tlv_decode_errors_total[5m])) 
            / 
            sum(rate(tlv_messages_total[5m]))
          ) > 0.05
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "TLV解码错误率超过5%"
          description: "当前错误率: {{ $value | humanizePercentage }}"
          runbook_url: "https://wiki.internal/tlv/decode-errors"

      # 版本不匹配
      - alert: TLVVersionMismatch
        expr: increase(tlv_version_mismatch_total[1h]) > 100
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "TLV版本不匹配事件增加"
          action: "检查是否需要协议升级"

      # 消息延迟过高
      - alert: TLVHighLatency
        expr: |
          histogram_quantile(0.99, 
            sum(rate(tlv_message_duration_seconds_bucket[5m])) by (le)
          ) > 0.5
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "TLV消息处理延迟过高"
          description: "P99延迟: {{ $value }}s"

      # 车辆批量离线
      - alert: TLVMassOffline
        expr: |
          (
            sum(tlv_vehicle_online) 
            / 
            sum(tlv_vehicle_total)
          ) < 0.8
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "超过20%车辆离线"
          action: "检查MQTT Broker和网络"
```

### 5.2 Grafana仪表板

```json
{
  "dashboard": {
    "title": "TLV协议健康监控",
    "panels": [
      {
        "title": "实时消息速率",
        "type": "graph",
        "targets": [
          {
            "expr": "sum(rate(tlv_messages_total[1m])) by (message_type)",
            "legendFormat": "{{ message_type }}"
          }
        ]
      },
      {
        "title": "错误分布",
        "type": "piechart",
        "targets": [
          {
            "expr": "sum(tlv_errors_total) by (error_code)",
            "legendFormat": "{{ error_code }}"
          }
        ]
      },
      {
        "title": "在线车辆数",
        "type": "stat",
        "targets": [
          {
            "expr": "sum(tlv_vehicle_online)",
            "instant": true
          }
        ]
      }
    ]
  }
}
```

---

## 6. 紧急恢复程序

### 6.1 协议回滚

```bash
#!/bin/bash
# emergency_rollback.sh

VERSION="$1"
if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 1.1.0"
    exit 1
fi

echo "!!! 执行紧急协议版本回滚 !!!"
echo "目标版本: $VERSION"
echo ""

# 1. 备份当前配置
cp /etc/tlv/config.yaml /etc/tlv/config.yaml.backup.$(date +%s)

# 2. 修改配置
sed -i "s/protocol_version:.*/protocol_version: $VERSION/" /etc/tlv/config.yaml

# 3. 滚动重启 (Kubernetes)
kubectl set image deployment/tlv-gateway \
    gateway=automotive/tlv-gateway:v$VERSION

# 4. 验证回滚
sleep 10
curl -s http://gateway:8080/health | jq '.version'

echo ""
echo "回滚完成，请监控日志"
```

### 6.2 消息积压清理

```bash
#!/bin/bash
# clear_message_backlog.sh

echo "=== 清理消息积压 ==="

# 查看积压
redis-cli LLEN tlv:queue:pending

# 导出积压消息 (备份)
redis-cli LRANGE tlv:queue:pending 0 -1 > /backup/backlog_$(date +%s).json

# 清空队列 (谨慎操作!)
read -p "确认清空积压队列? (yes/no): " confirm
if [ "$confirm" = "yes" ]; then
    redis-cli DEL tlv:queue:pending
    echo "队列已清空"
fi
```

---

*故障排查手册版本: v1.0.0*  
*最后更新: 2026-05-11*
