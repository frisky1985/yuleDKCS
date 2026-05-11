# BER-TLV 参考实现指南

> **版本**: v1.0.0  
> **协议版本**: v1.2.0  
> **目标**: 可立即编译运行的完整实现

---

## 1. 项目结构

```
tlv-reference-implementation/
├── README.md                      # 快速开始指南
├── Makefile                       # 顶层构建
├── docker-compose.yml             # 本地开发环境
├── schema/
│   └── tlv_protocol_v1.2.0.yaml     # 协议Schema
├── src/
│   ├── c/                         # C参考实现
│   │   ├── include/
│   │   │   ├── tlv_codec.h
│   │   │   └── tlv_types.h
│   │   ├── src/
│   │   │   ├── tlv_encoder.c
│   │   │   ├── tlv_decoder.c
│   │   │   └── tlv_utils.c
│   │   ├── tests/
│   │   │   └── test_tlv.c
│   │   ├── examples/
│   │   │   ├── encode_heartbeat.c
│   │   │   └── decode_message.c
│   │   ├── Makefile
│   │   └── CMakeLists.txt
│   ├── java/                        # Java参考实现
│   │   ├── src/main/java/com/automotive/tlv/
│   │   │   ├── TlvEncoder.java
│   │   │   ├── TlvDecoder.java
│   │   │   ├── TlvMessage.java
│   │   │   └── TlvException.java
│   │   ├── src/test/java/
│   │   ├── pom.xml
│   │   └── README.md
│   ├── python/                      # Python参考实现
│   │   ├── tlv_protocol/
│   │   │   ├── __init__.py
│   │   │   ├── encoder.py
│   │   │   ├── decoder.py
│   │   │   └── validator.py
│   │   ├── tests/
│   │   ├── setup.py
│   │   └── requirements.txt
│   ├── typescript/                  # TypeScript参考实现
│   │   ├── src/
│   │   │   ├── index.ts
│   │   │   ├── encoder.ts
│   │   │   └── decoder.ts
│   │   ├── tests/
│   │   ├── package.json
│   │   └── tsconfig.json
│   └── swift/                       # Swift参考实现
│       ├── Sources/TLVProtocol/
│       │   ├── TLVProtocol.swift
│       │   ├── TLVEncoder.swift
│       │   └── TLVDecoder.swift
│       ├── Tests/
│       └── Package.swift
├── interop/                       # 互操作性测试
│   ├── test_suite.py
│   ├── test_cases/
│   └── README.md
├── benchmarks/                    # 性能测试
│   ├── run_benchmarks.sh
│   └── results/
└── .github/
    └── workflows/
        └── ci.yml                   # CI配置
```

---

## 2. 快速开始

### 2.1 克隆和构建

```bash
# 克隆仓库
git clone https://github.com/automotive/tlv-reference-implementation.git
cd tlv-reference-implementation

# 一键构建所有语言实现
make all

# 运行测试
make test

# 运行兼容性测试
make interop
```

### 2.2 Docker快速启动

```bash
# 启动完整开发环境
docker-compose up -d

# 进入开发容器
docker-compose exec dev bash

# 在容器内构建
make all && make test
```

---

## 3. 各语言快速使用

### 3.1 C语言

```c
// 编码示例
#include "tlv_codec.h"

int main() {
    // 创建缓冲区
    uint8_t buffer[256];
    TlvEncoder encoder;
    tlv_encoder_init(&encoder, buffer, sizeof(buffer));
    
    // 编码消息
    tlv_encode_version(&encoder, 1, 2, 0);
    tlv_encode_message_type(&encoder, MSG_HEARTBEAT);
    tlv_encode_timestamp(&encoder, get_timestamp_ms());
    tlv_encode_u64(&encoder, TAG_UPTIME, 86400000);
    
    // 获取结果
    size_t len = tlv_encoder_finish(&encoder);
    
    // 打印HEX
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", buffer[i]);
    }
    
    return 0;
}
```

**Build & Run:**
```bash
cd src/c
make example
./encode_heartbeat
# 输出: 80 03 01 02 00 81 01 05 83 08 01 77 8D 66 4D 4E 01 00 ...
```

### 3.2 Java

```java
import com.automotive.tlv.*;

public class QuickStart {
    public static void main(String[] args) {
        // 创建编码器
        TlvEncoder encoder = new TlvEncoder(256);
        
        // 编码消息
        encoder.encodeVersion(1, 2, 0)
               .encodeMessageType(MessageType.HEARTBEAT)
               .encodeTimestamp(System.currentTimeMillis())
               .encodeU64(Tag.UPTIME, 86400000L);
        
        // 获取字节数组
        byte[] data = encoder.toByteArray();
        
        // 解码
        TlvDecoder decoder = new TlvDecoder(data);
        TlvMessage msg = decoder.decode();
        
        System.out.println("Version: " + msg.getVersion());
        System.out.println("Type: " + msg.getMessageType());
    }
}
```

**Build & Run:**
```bash
cd src/java
mvn compile exec:java -Dexec.mainClass="QuickStart"
```

### 3.3 Python

```python
from tlv_protocol import TlvEncoder, TlvDecoder
import time

# 编码
encoder = TlvEncoder()
encoder.encode_version(1, 2, 0)
encoder.encode_message_type("HEARTBEAT")
encoder.encode_timestamp(int(time.time() * 1000))
encoder.encode_u64("UPTIME", 86400000)

data = encoder.finish()
print(f"Encoded: {data.hex()}")

# 解码
decoder = TlvDecoder(data)
msg = decoder.decode()
print(f"Message: {msg}")
```

**Run:**
```bash
cd src/python
pip install -e .
python quickstart.py
```

### 3.4 TypeScript

```typescript
import { TlvEncoder, TlvDecoder } from 'tlv-protocol';

// 编码
const encoder = new TlvEncoder();
encoder.encodeVersion({ major: 1, minor: 2, patch: 0 })
       .encodeMessageType(MessageType.HEARTBEAT)
       .encodeTimestamp(BigInt(Date.now()));

const data = encoder.finish();
console.log('Encoded:', Buffer.from(data).toString('hex'));

// 解码
const decoder = new TlvDecoder(data);
const msg = decoder.decode();
console.log('Decoded:', msg);
```

### 3.5 Swift

```swift
import TLVProtocol

// 编码
var encoder = TLVEncoder()
encoder.encodeVersion(major: 1, minor: 2, patch: 0)
encoder.encodeMessageType(.heartbeat)
encoder.encodeTimestamp(UInt64(Date().timeIntervalSince1970 * 1000))

let data = encoder.finish()
print("Encoded: \(data.map { String(format: "%02X", $0) }.joined())")

// 解码
var decoder = TLVDecoder(data)
let msg = try decoder.decode()
print("Decoded: \(msg)")
```

---

## 4. 完整使用示例

### 4.1 车辆心跳上报 (C实现)

```c
// examples/vehicle_heartbeat.c
#include "tlv_codec.h"
#include <time.h>
#include <unistd.h>

void send_heartbeat() {
    uint8_t buffer[256];
    TlvEncoder encoder;
    
    static uint32_t sequence = 0;
    
    tlv_encoder_init(&encoder, buffer, sizeof(buffer));
    
    // 必须字段
    tlv_encode_version(&encoder, 1, 2, 0);
    tlv_encode_message_type(&encoder, MSG_HEARTBEAT);
    tlv_encode_message_id(&encoder, sequence++);
    tlv_encode_timestamp(&encoder, time_ms());
    
    // 车端状态
    tlv_encode_u64(&encoder, TAG_UPTIME, get_uptime_ms());
    tlv_encode_u8(&encoder, TAG_SIGNAL_STRENGTH, get_signal_strength());
    tlv_encode_u8(&encoder, TAG_BATTERY_LEVEL, get_battery_level());
    tlv_encode_double(&encoder, TAG_GPS_LATITUDE, get_latitude());
    tlv_encode_double(&encoder, TAG_GPS_LONGITUDE, get_longitude());
    
    size_t len = tlv_encoder_finish(&encoder);
    
    // 发送到MQTT
    mqtt_publish("vehicle/heartbeat", buffer, len);
}

int main() {
    while (1) {
        send_heartbeat();
        sleep(30);  // 30秒间隔
    }
    return 0;
}
```

### 4.2 云端命令处理 (Java实现)

```java
// examples/CloudCommandProcessor.java
import com.automotive.tlv.*;
import org.eclipse.paho.client.mqttv3.*;

public class CloudCommandProcessor implements MqttCallback {
    
    public void processCommand(byte[] payload) {
        try {
            TlvDecoder decoder = new TlvDecoder(payload);
            TlvMessage msg = decoder.decode();
            
            // 验证版本
            if (!msg.getVersion().equals("1.2.0")) {
                sendError(msg, ErrorCode.UNSUPPORTED_VERSION);
                return;
            }
            
            // 验证权限
            if (!authenticate(msg.getSessionId())) {
                sendError(msg, ErrorCode.AUTHENTICATION_FAILED);
                return;
            }
            
            // 执行命令
            switch (msg.getMessageType()) {
                case COMMAND:
                    handleCommand(msg);
                    break;
                case SYNC_REQUEST:
                    handleSync(msg);
                    break;
                default:
                    sendError(msg, ErrorCode.INVALID_COMMAND);
            }
            
        } catch (TlvException e) {
            logger.error("TLV decode error", e);
        }
    }
    
    private void handleCommand(TlvMessage msg) {
        String vin = msg.getString(TAG_VIN);
        CommandType cmd = msg.getEnum(TAG_COMMAND_TYPE);
        
        // 转发到车辆
        forwardToVehicle(vin, msg);
        
        // 发送确认
        sendAck(msg);
    }
}
```

### 4.3 手机SDK集成 (Swift实现)

```swift
// examples/MobileSDKExample.swift
import TLVProtocol
import Foundation

class VehicleControlSDK {
    private var client: TLVProtocolClient
    
    func lockVehicle(vin: String) async throws -> CommandResult {
        // 构建命令
        var encoder = TLVEncoder()
        encoder.encodeVersion(major: 1, minor: 2, patch: 0)
        encoder.encodeMessageType(.command)
        encoder.encodeString(tag: .vin, value: vin)
        encoder.encodeEnum(tag: .commandType, value: .doorLock)
        
        let data = encoder.finish()
        
        // 发送请求
        let response = try await client.sendCommand(data)
        
        // 解析响应
        var decoder = TLVDecoder(response)
        let result = try decoder.decode()
        
        return CommandResult(
            success: result.messageType == .commandAck,
            timestamp: Date()
        )
    }
}
```

---

## 5. 测试验证

### 5.1 运行测试套件

```bash
# 单元测试
make test

# 互操作性测试
make interop

# 性能测试
make benchmark

# 所有测试
make test-all
```

### 5.2 验证三端互通

```bash
# 1. 启动模拟环境
docker-compose up -d mqtt redis

# 2. 启动云端
docker-compose up -d cloud-gateway

# 3. 运行车端模拟
docker-compose run --rm vehicle-simulator

# 4. 运行手机SDK测试
docker-compose run --rm mobile-sdk-test

# 5. 查看测试报告
cat reports/interop_report.json
```

---

## 6. 性能基准

### 6.1 运行性能测试

```bash
# C实现性能
./benchmarks/c_benchmark
cd src/c && make benchmark && ./benchmark

# 预期输出:
# Encode latency: 12.5 μs (p99)
# Decode latency: 15.2 μs (p99)
# Throughput: 82,000 msg/s

# Java实现性能
cd src/java && mvn exec:java -Dexec.mainClass="Benchmark"

# 预期输出:
# Encode latency: 8.3 μs (p99)
# Decode latency: 12.1 μs (p99)
# Throughput: 125,000 msg/s
```

### 6.2 对比测试

```bash
# 与JSON对比
python3 benchmarks/compare_with_json.py

# 输出:
# TLV size: 45 bytes
# JSON size: 186 bytes
# Compression ratio: 75.8%
```

---

*参考实现指南版本: v1.0.0*

完整实现代码请访问: https://github.com/automotive/tlv-reference-implementation
