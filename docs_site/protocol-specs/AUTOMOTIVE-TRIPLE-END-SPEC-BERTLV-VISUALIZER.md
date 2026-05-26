# BER-TLV 可视化抓包分析器

> **版本**: v1.0.0  
> **协议版本**: v1.2.0  
> **支持协议**: MQTT, HTTP/2, WebSocket, 原始TCP

---

## 1. 工具概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TLV Protocol Visualizer                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌───────────────────────────────────────────────────────────────────────┐         │
│  │                     抓包模块                                  │         │
│  ├───────────────────────────────────────────────────────────────────────┤         │
│  │  • libpcap/tcpdump - 原始包捕获                                │         │
│  │  • MQTT proxy - 中间人模式                                    │         │
│  │  • TLS decryption - 密钥解密(需要私钥)                      │         │
│  │  • Pcap file import - 文件回放                                 │         │
│  └───────────────────────────────────────────────────────────────────────┘         │
│                                    │                                     │
│                                    ▼                                     │
│  ┌───────────────────────────────────────────────────────────────────────┐         │
│  │                  协议解析模块                                │         │
│  ├───────────────────────────────────────────────────────────────────────┤         │
│  │  • 层次化解析: Ethernet → IP → TCP → TLS → MQTT → TLV       │         │
│  │  • 字段识别: 自动识别协议版本和TAG类型                      │         │
│  │  • 校验和验证: 实时验证消息完整性                          │         │
│  │  • 时序分析: Request/Response匹配、Latency计算                │         │
│  └───────────────────────────────────────────────────────────────────────┘         │
│                                    │                                     │
│                                    ▼                                     │
│  ┌───────────────────────────────────────────────────────────────────────┐         │
│  │                    可视化界面                                │         │
│  ├───────────────────────────────────────────────────────────────────────┤         │
│  │  • Web界面 (React/Vue)                                       │         │
│  │  • Electron桌面应用                                        │         │
│  │  • 实时流量图表                                              │         │
│  │  • 消息时序图 (Sequence Diagram)                             │         │
│  │  • TLV树状结构可视化                                        │         │
│  └───────────────────────────────────────────────────────────────────────┘         │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 核心组件实现

### 2.1 抓包引擎 (Python)

```python
#!/usr/bin/env python3
"""
TLV Protocol Packet Analyzer
支持实时抓包和文件分析
"""

import asyncio
import json
import struct
from dataclasses import dataclass, asdict
from datetime import datetime
from typing import Optional, Dict, List, Callable, BinaryIO
from enum import Enum
import socket
import ssl

try:
    import scapy.all as scapy
    SCAPY_AVAILABLE = True
except ImportError:
    SCAPY_AVAILABLE = False


class ProtocolLayer(Enum):
    """协议层次"""
    ETHERNET = 1
    IP = 2
    TCP = 3
    TLS = 4
    MQTT = 5
    TLV = 6


@dataclass
class PacketInfo:
    """报文信息"""
    timestamp: float
    src_addr: str
    dst_addr: str
    src_port: int
    dst_port: int
    protocol: str
    raw_data: bytes
    layers: Dict[str, any]
    tlv_messages: List['TlvMessage']
    

@dataclass
class TlvMessage:
    """TLV解析结果"""
    offset: int
    elements: List['TlvElement']
    validation_result: 'ValidationResult'
    protocol_version: Optional[str]
    message_type: Optional[str]
    

@dataclass
class TlvElement:
    """TLV元素"""
    tag: int
    tag_name: str
    length: int
    value: bytes
    value_preview: str
    offset: int


@dataclass
class ValidationResult:
    """验证结果"""
    valid: bool
    errors: List[str]
    warnings: List[str]


class TlvParser:
    """TLV协议解析器"""
    
    TAG_NAMES = {
        0x80: "PROTOCOL_VERSION",
        0x81: "MESSAGE_TYPE",
        0x82: "MESSAGE_ID",
        0x83: "TIMESTAMP",
        0x84: "SESSION_ID",
        0x01: "BOOLEAN",
        0x02: "INTEGER",
        0x0C: "UTF8STRING",
        0x04: "OCTET_STRING"
    }
    
    MESSAGE_TYPES = {
        0x01: "REQUEST",
        0x02: "RESPONSE",
        0x04: "NOTIFICATION",
        0x05: "HEARTBEAT",
        0x07: "COMMAND"
    }
    
    def parse(self, data: bytes) -> TlvMessage:
        """解析TLV数据"""
        elements = []
        offset = 0
        errors = []
        warnings = []
        
        protocol_version = None
        message_type = None
        
        while offset < len(data):
            try:
                if offset >= len(data):
                    break
                
                tag = data[offset]
                tag_start = offset
                offset += 1
                
                # 解码长度
                length, offset = self._decode_length(data, offset)
                
                if offset + length > len(data):
                    errors.append(f"Tag 0x{tag:02X}: Length {length} exceeds buffer at offset {offset}")
                    break
                
                value = data[offset:offset+length]
                
                # 生成预览
                preview = self._generate_preview(tag, value)
                
                element = TlvElement(
                    tag=tag,
                    tag_name=self.TAG_NAMES.get(tag, f"TAG_0x{tag:02X}"),
                    length=length,
                    value=value,
                    value_preview=preview,
                    offset=tag_start
                )
                elements.append(element)
                
                # 提取元信息
                if tag == 0x80 and length == 3:
                    protocol_version = f"{value[0]}.{value[1]}.{value[2]}"
                elif tag == 0x81 and length == 1:
                    message_type = self.MESSAGE_TYPES.get(value[0], f"0x{value[0]:02X}")
                
                offset += length
                
            except Exception as e:
                errors.append(f"Parse error at offset {offset}: {e}")
                break
        
        # 验证
        validation = self._validate(elements)
        
        return TlvMessage(
            offset=0,
            elements=elements,
            validation_result=validation,
            protocol_version=protocol_version,
            message_type=message_type
        )
    
    def _decode_length(self, data: bytes, offset: int) -> tuple:
        """解码BER长度"""
        if offset >= len(data):
            raise ValueError("Unexpected end of data")
        
        first = data[offset]
        offset += 1
        
        if (first & 0x80) == 0:
            return first, offset
        
        num_bytes = first & 0x7F
        if num_bytes == 0:
            raise ValueError("Indefinite length not supported")
        if num_bytes > 4:
            raise ValueError("Length too large")
        
        length = 0
        for _ in range(num_bytes):
            length = (length << 8) | data[offset]
            offset += 1
        
        return length, offset
    
    def _generate_preview(self, tag: int, value: bytes) -> str:
        """生成值预览"""
        if tag == 0x80 and len(value) == 3:
            return f"v{value[0]}.{value[1]}.{value[2]}"
        elif tag == 0x81 and len(value) == 1:
            return self.MESSAGE_TYPES.get(value[0], f"0x{value[0]:02X}")
        elif tag == 0x83 and len(value) == 8:
            ts = struct.unpack('>Q', value)[0]
            dt = datetime.fromtimestamp(ts / 1000)
            return f"{ts} ({dt.strftime('%Y-%m-%d %H:%M:%S')})"
        elif tag in [0x0C]:  # UTF8String
            try:
                return value.decode('utf-8')[:50]
            except:
                pass
        
        # Hex preview
        hex_str = value.hex()
        if len(hex_str) > 50:
            return hex_str[:50] + "..."
        return hex_str
    
    def _validate(self, elements: List[TlvElement]) -> ValidationResult:
        """验证TLV消息"""
        errors = []
        warnings = []
        
        tags = {e.tag for e in elements}
        
        # 检查必需字段
        required = {0x80, 0x81, 0x82, 0x83}
        missing = required - tags
        if missing:
            for tag in missing:
                errors.append(f"Missing required tag: 0x{tag:02X} ({self.TAG_NAMES.get(tag, 'Unknown')})")
        
        # 检查版本
        for elem in elements:
            if elem.tag == 0x80:
                if elem.length != 3:
                    errors.append("Protocol version must be 3 bytes")
                elif elem.value[0] != 1 or elem.value[1] != 2:
                    warnings.append(f"Version {elem.value[0]}.{elem.value[1]}.{elem.value[2]} may not be fully supported")
        
        return ValidationResult(
            valid=len(errors) == 0,
            errors=errors,
            warnings=warnings
        )


class MqttDissector:
    """MQTT协议解析"""
    
    PACKET_TYPES = {
        1: "CONNECT",
        2: "CONNACK",
        3: "PUBLISH",
        4: "PUBACK",
        5: "PUBREC",
        6: "PUBREL",
        7: "PUBCOMP",
        8: "SUBSCRIBE",
        9: "SUBACK",
        10: "UNSUBSCRIBE",
        11: "UNSUBACK",
        12: "PINGREQ",
        13: "PINGRESP",
        14: "DISCONNECT"
    }
    
    def dissect(self, data: bytes) -> Optional[Dict]:
        """解析MQTT报文"""
        if len(data) < 2:
            return None
        
        packet_type = (data[0] >> 4) & 0x0F
        flags = data[0] & 0x0F
        
        result = {
            "type": self.PACKET_TYPES.get(packet_type, f"UNKNOWN({packet_type})"),
            "flags": flags,
            "remaining_length": self._decode_remaining_length(data[1:])
        }
        
        # 如果是PUBLISH包，尝试解析主题和TLV载荷
        if packet_type == 3 and len(data) > 5:
            try:
                topic, payload = self._parse_publish(data)
                result["topic"] = topic
                
                # 尝试解析TLV
                tlv_parser = TlvParser()
                tlv_msg = tlv_parser.parse(payload)
                if tlv_msg.elements:
                    result["tlv"] = tlv_msg
            except Exception as e:
                result["parse_error"] = str(e)
        
        return result
    
    def _decode_remaining_length(self, data: bytes) -> int:
        """解码MQTT变长长度"""
        multiplier = 1
        value = 0
        idx = 0
        
        while True:
            if idx >= len(data):
                break
            byte = data[idx]
            value += (byte & 0x7F) * multiplier
            multiplier *= 128
            idx += 1
            if (byte & 0x80) == 0:
                break
        
        return value
    
    def _parse_publish(self, data: bytes) -> tuple:
        """解析PUBLISH包"""
        idx = 1
        # Skip remaining length
        while data[idx] & 0x80:
            idx += 1
        idx += 1
        
        # Topic length
        topic_len = struct.unpack('>H', data[idx:idx+2])[0]
        idx += 2
        
        # Topic
        topic = data[idx:idx+topic_len].decode('utf-8')
        idx += topic_len
        
        # Packet ID (if QoS > 0)
        qos = (data[0] >> 1) & 0x03
        if qos > 0:
            idx += 2
        
        # Payload
        payload = data[idx:]
        return topic, payload


class PacketCapture:
    """报文捕获器"""
    
    def __init__(self):
        self.packets: List[PacketInfo] = []
        self.callbacks: List[Callable[[PacketInfo], None]] = []
        self.running = False
    
    def add_callback(self, callback: Callable[[PacketInfo], None]):
        """添加报文处理回调"""
        self.callbacks.append(callback)
    
    def capture_from_pcap(self, filename: str):
        """从pcap文件读取"""
        if not SCAPY_AVAILABLE:
            raise ImportError("Scapy is required for pcap parsing")
        
        packets = scapy.rdpcap(filename)
        
        for pkt in packets:
            self._process_scapy_packet(pkt)
    
    def capture_live(self, interface: str, filter_expr: str = "tcp port 1883 or tcp port 8883"):
        """实时捕获"""
        if not SCAPY_AVAILABLE:
            raise ImportError("Scapy is required for live capture")
        
        self.running = True
        
        def packet_handler(pkt):
            if not self.running:
                return
            self._process_scapy_packet(pkt)
        
        scapy.sniff(
            iface=interface,
            filter=filter_expr,
            prn=packet_handler,
            store=0
        )
    
    def capture_from_proxy(self, local_port: int, remote_host: str, remote_port: int):
        """代理模式捕获"""
        asyncio.run(self._run_proxy(local_port, remote_host, remote_port))
    
    async def _run_proxy(self, local_port: int, remote_host: str, remote_port: int):
        """运行TCP代理"""
        server = await asyncio.start_server(
            lambda r, w: self._handle_proxy_client(r, w, remote_host, remote_port),
            '0.0.0.0', local_port
        )
        
        print(f"Proxy listening on port {local_port} -> {remote_host}:{remote_port}")
        
        async with server:
            await server.serve_forever()
    
    async def _handle_proxy_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter, 
                                   remote_host: str, remote_port: int):
        """处理代理连接"""
        remote_reader, remote_writer = await asyncio.open_connection(remote_host, remote_port)
        
        async def forward(src: asyncio.StreamReader, dst: asyncio.StreamWriter, direction: str):
            while True:
                data = await src.read(4096)
                if not data:
                    break
                
                # 解析并通知
                self._analyze_proxy_data(data, direction)
                
                dst.write(data)
                await dst.drain()
        
        await asyncio.gather(
            forward(reader, remote_writer, "C->S"),
            forward(remote_reader, writer, "S->C")
        )
        
        writer.close()
        remote_writer.close()
    
    def _analyze_proxy_data(self, data: bytes, direction: str):
        """分析代理数据"""
        # 尝试解析MQTT
        mqtt = MqttDissector()
        mqtt_result = mqtt.dissect(data)
        
        if mqtt_result and "tlv" in mqtt_result:
            info = PacketInfo(
                timestamp=datetime.now().timestamp(),
                src_addr="client" if direction == "C->S" else "server",
                dst_addr="server" if direction == "C->S" else "client",
                src_port=0,
                dst_port=0,
                protocol="MQTT+TLV",
                raw_data=data,
                layers={"mqtt": mqtt_result},
                tlv_messages=[mqtt_result["tlv"]]
            )
            
            self.packets.append(info)
            for cb in self.callbacks:
                cb(info)
    
    def _process_scapy_packet(self, pkt):
        """处理Scapy报文"""
        if not pkt.haslayer(scapy.TCP):
            return
        
        tcp = pkt[scapy.TCP]
        ip = pkt[scapy.IP]
        
        # 提取载荷
        payload = bytes(tcp.payload)
        if not payload:
            return
        
        # 尝试解析MQTT
        mqtt = MqttDissector()
        mqtt_result = mqtt.dissect(payload)
        
        tlv_messages = []
        if mqtt_result and "tlv" in mqtt_result:
            tlv_messages.append(mqtt_result["tlv"])
        
        info = PacketInfo(
            timestamp=float(pkt.time),
            src_addr=ip.src,
            dst_addr=ip.dst,
            src_port=tcp.sport,
            dst_port=tcp.dport,
            protocol="MQTT" if mqtt_result else "TCP",
            raw_data=payload,
            layers={"mqtt": mqtt_result} if mqtt_result else {},
            tlv_messages=tlv_messages
        )
        
        self.packets.append(info)
        for cb in self.callbacks:
            cb(info)
    
    def stop(self):
        """停止捕获"""
        self.running = False
    
    def export_json(self, filename: str):
        """导出为JSON"""
        data = []
        for pkt in self.packets:
            pkt_dict = asdict(pkt)
            # Convert bytes to hex
            pkt_dict['raw_data'] = pkt.raw_data.hex()
            for tlv in pkt_dict.get('tlv_messages', []):
                if hasattr(tlv, 'elements'):
                    for elem in tlv.elements:
                        elem.value = elem.value.hex()
            data.append(pkt_dict)
        
        with open(filename, 'w') as f:
            json.dump(data, f, indent=2)
    
    def get_statistics(self) -> Dict:
        """获取统计信息"""
        stats = {
            "total_packets": len(self.packets),
            "protocols": {},
            "message_types": {},
            "errors": 0,
            "warnings": 0
        }
        
        for pkt in self.packets:
            # 协议统计
            proto = pkt.protocol
            stats["protocols"][proto] = stats["protocols"].get(proto, 0) + 1
            
            # TLV消息统计
            for tlv in pkt.tlv_messages:
                if tlv.message_type:
                    stats["message_types"][tlv.message_type] = \
                        stats["message_types"].get(tlv.message_type, 0) + 1
                
                if not tlv.validation_result.valid:
                    stats["errors"] += 1
                stats["warnings"] += len(tlv.validation_result.warnings)
        
        return stats


# CLI入口
def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='TLV Protocol Analyzer')
    parser.add_argument('--pcap', help='Pcap file to analyze')
    parser.add_argument('--interface', '-i', help='Network interface for live capture')
    parser.add_argument('--proxy-port', type=int, help='Proxy port')
    parser.add_argument('--proxy-target', help='Proxy target host:port')
    parser.add_argument('--output', '-o', help='Output JSON file')
    parser.add_argument('--filter', default='tcp port 1883 or tcp port 8883', help='BPF filter')
    
    args = parser.parse_args()
    
    capture = PacketCapture()
    
    # 添加打印回调
    def print_packet(info: PacketInfo):
        print(f"\n[{datetime.fromtimestamp(info.timestamp)}]")
        print(f"{info.src_addr}:{info.src_port} -> {info.dst_addr}:{info.dst_port}")
        print(f"Protocol: {info.protocol}")
        
        for tlv in info.tlv_messages:
            print(f"TLV Version: {tlv.protocol_version}, Type: {tlv.message_type}")
            print(f"Elements: {len(tlv.elements)}")
            for elem in tlv.elements:
                print(f"  0x{elem.tag:02X} ({elem.tag_name}): {elem.value_preview}")
            
            if not tlv.validation_result.valid:
                print(f"Validation Errors: {tlv.validation_result.errors}")
    
    capture.add_callback(print_packet)
    
    if args.pcap:
        print(f"Reading pcap file: {args.pcap}")
        capture.capture_from_pcap(args.pcap)
    elif args.interface:
        print(f"Capturing on interface: {args.interface}")
        capture.capture_live(args.interface, args.filter)
    elif args.proxy_port and args.proxy_target:
        host, port = args.proxy_target.split(':')
        print(f"Starting proxy on port {args.proxy_port} -> {host}:{port}")
        capture.capture_from_proxy(args.proxy_port, host, int(port))
    else:
        parser.print_help()
        return
    
    # 打印统计
    stats = capture.get_statistics()
    print("\n" + "="*50)
    print("Statistics:")
    print(json.dumps(stats, indent=2))
    
    if args.output:
        capture.export_json(args.output)
        print(f"\nExported to: {args.output}")


if __name__ == "__main__":
    main()
```

---

## 3. Web可视化界面

### 3.1 React前端

```typescript
// src/components/TlvVisualizer.tsx
import React, { useState, useEffect, useRef } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';

interface TlvElement {
  tag: number;
  tag_name: string;
  length: number;
  value_preview: string;
}

interface TlvMessage {
  protocol_version: string;
  message_type: string;
  elements: TlvElement[];
  validation_result: {
    valid: boolean;
    errors: string[];
    warnings: string[];
  };
}

interface Packet {
  timestamp: number;
  src_addr: string;
  dst_addr: string;
  protocol: string;
  tlv_messages: TlvMessage[];
}

const TlvVisualizer: React.FC = () => {
  const [packets, setPackets] = useState<Packet[]>([]);
  const [selectedPacket, setSelectedPacket] = useState<Packet | null>(null);
  const [isCapturing, setIsCapturing] = useState(false);
  const [filter, setFilter] = useState('');
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    // Connect to WebSocket server
    wsRef.current = new WebSocket('ws://localhost:8765');
    
    wsRef.current.onmessage = (event) => {
      const packet: Packet = JSON.parse(event.data);
      setPackets(prev => [...prev.slice(-1000), packet]);
    };

    return () => {
      wsRef.current?.close();
    };
  }, []);

  const startCapture = () => {
    setIsCapturing(true);
    wsRef.current?.send(JSON.stringify({ action: 'start' }));
  };

  const stopCapture = () => {
    setIsCapturing(false);
    wsRef.current?.send(JSON.stringify({ action: 'stop' }));
  };

  const filteredPackets = packets.filter(p => {
    if (!filter) return true;
    const searchStr = filter.toLowerCase();
    return p.src_addr.toLowerCase().includes(searchStr) ||
           p.dst_addr.toLowerCase().includes(searchStr) ||
           p.protocol.toLowerCase().includes(searchStr) ||
           p.tlv_messages.some(t => 
             t.message_type.toLowerCase().includes(searchStr)
           );
  });

  return (
    <div className="tlv-visualizer">
      <header className="toolbar">
        <h1>TLV Protocol Visualizer</h1>
        <div className="controls">
          <button 
            onClick={isCapturing ? stopCapture : startCapture}
            className={isCapturing ? 'stop' : 'start'}
          >
            {isCapturing ? '⏹ Stop' : '▶ Start'}
          </button>
          <input
            type="text"
            placeholder="Filter..."
            value={filter}
            onChange={(e) => setFilter(e.target.value)}
          />
          <span className="packet-count">{packets.length} packets</span>
        </div>
      </header>

      <div className="main-content">
        <div className="packet-list">
          <table>
            <thead>
              <tr>
                <th>Time</th>
                <th>Source</th>
                <th>Destination</th>
                <th>Protocol</th>
                <th>Type</th>
                <th>Status</th>
              </tr>
            </thead>
            <tbody>
              {filteredPackets.slice().reverse().map((pkt, idx) => (
                <tr 
                  key={idx}
                  onClick={() => setSelectedPacket(pkt)}
                  className={selectedPacket === pkt ? 'selected' : ''}
                >
                  <td>{new Date(pkt.timestamp * 1000).toLocaleTimeString()}</td>
                  <td>{pkt.src_addr}</td>
                  <td>{pkt.dst_addr}</td>
                  <td>{pkt.protocol}</td>
                  <td>{pkt.tlv_messages[0]?.message_type || '-'}</td>
                  <td>
                    {pkt.tlv_messages[0]?.validation_result.valid ? 
                      <span className="badge valid">OK</span> :
                      <span className="badge error">ERR</span>
                    }
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        <div className="detail-panel">
          {selectedPacket ? (
            <PacketDetail packet={selectedPacket} />
          ) : (
            <div className="no-selection">Select a packet to view details</div>
          )}
        </div>
      </div>

      <div className="statistics-panel">
        <TrafficChart packets={packets} />
      </div>
    </div>
  );
};

const PacketDetail: React.FC<{ packet: Packet }> = ({ packet }) => {
  const tlv = packet.tlv_messages[0];
  
  return (
    <div className="packet-detail">
      <h3>Packet Details</h3>
      <div className="info-grid">
        <label>Timestamp:</label>
        <value>{new Date(packet.timestamp * 1000).toISOString()}</value>
        
        <label>Source:</label>
        <value>{packet.src_addr}</value>
        
        <label>Destination:</label>
        <value>{packet.dst_addr}</value>
        
        <label>Protocol:</label>
        <value>{packet.protocol}</value>
      </div>

      {tlv && (
        <>
          <h4>TLV Message</h4>
          <div className="tlv-header">
            <span>Version: {tlv.protocol_version}</span>
            <span>Type: {tlv.message_type}</span>
            <span className={tlv.validation_result.valid ? 'valid' : 'invalid'}>
              {tlv.validation_result.valid ? '✓ Valid' : '✗ Invalid'}
            </span>
          </div>

          <h5>Elements</h5>
          <div className="tlv-tree">
            {tlv.elements.map((elem, idx) => (
              <div key={idx} className="tlv-element">
                <span className="tag">0x{elem.tag.toString(16).padStart(2, '0')}</span>
                <span className="tag-name">{elem.tag_name}</span>
                <span className="length">({elem.length} bytes)</span>
                <span className="value">{elem.value_preview}</span>
              </div>
            ))}
          </div>

          {tlv.validation_result.errors.length > 0 && (
            <div className="validation-errors">
              <h5>Errors</h5>
              <ul>
                {tlv.validation_result.errors.map((err, idx) => (
                  <li key={idx} className="error">{err}</li>
                ))}
              </ul>
            </div>
          )}

          {tlv.validation_result.warnings.length > 0 && (
            <div className="validation-warnings">
              <h5>Warnings</h5>
              <ul>
                {tlv.validation_result.warnings.map((warn, idx) => (
                  <li key={idx} className="warning">{warn}</li>
                ))}
              </ul>
            </div>
          )}
        </>
      )}
    </div>
  );
};

const TrafficChart: React.FC<{ packets: Packet[] }> = ({ packets }) => {
  // Group packets by 1-second buckets
  const data = packets.reduce((acc, pkt) => {
    const bucket = Math.floor(pkt.timestamp);
    acc[bucket] = (acc[bucket] || 0) + 1;
    return acc;
  }, {} as Record<number, number>);

  const chartData = Object.entries(data)
    .slice(-60)
    .map(([time, count]) => ({
      time: new Date(parseInt(time) * 1000).toLocaleTimeString(),
      packets: count
    }));

  return (
    <div className="traffic-chart">
      <h4>Traffic (pps)</h4>
      <ResponsiveContainer width="100%" height={150}>
        <LineChart data={chartData}>
          <CartesianGrid strokeDasharray="3 3" />
          <XAxis dataKey="time" tick={{fontSize: 10}} />
          <YAxis />
          <Tooltip />
          <Line type="monotone" dataKey="packets" stroke="#8884d8" />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
};

export default TlvVisualizer;
```

### 3.2 CSS样式

```css
/* src/styles/tlv-visualizer.css */

.tlv-visualizer {
  display: flex;
  flex-direction: column;
  height: 100vh;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
}

.toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 10px 20px;
  background: #1e1e1e;
  color: white;
  border-bottom: 1px solid #333;
}

.toolbar h1 {
  margin: 0;
  font-size: 18px;
}

.controls {
  display: flex;
  gap: 10px;
  align-items: center;
}

.controls button {
  padding: 6px 16px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-weight: 500;
}

.controls button.start {
  background: #4CAF50;
  color: white;
}

.controls button.stop {
  background: #f44336;
  color: white;
}

.controls input {
  padding: 6px 12px;
  border: 1px solid #444;
  border-radius: 4px;
  background: #2d2d2d;
  color: white;
}

.packet-count {
  color: #888;
  font-size: 12px;
}

.main-content {
  display: flex;
  flex: 1;
  overflow: hidden;
}

.packet-list {
  width: 50%;
  overflow: auto;
  background: #252526;
}

.packet-list table {
  width: 100%;
  border-collapse: collapse;
  font-size: 12px;
}

.packet-list th {
  position: sticky;
  top: 0;
  background: #333;
  padding: 8px;
  text-align: left;
  color: #ccc;
  font-weight: 500;
}

.packet-list td {
  padding: 6px 8px;
  border-bottom: 1px solid #333;
  color: #ddd;
  cursor: pointer;
}

.packet-list tr:hover {
  background: #2a2d2e;
}

.packet-list tr.selected {
  background: #094771;
}

.badge {
  padding: 2px 6px;
  border-radius: 3px;
  font-size: 10px;
  font-weight: bold;
}

.badge.valid {
  background: #4CAF50;
  color: white;
}

.badge.error {
  background: #f44336;
  color: white;
}

.detail-panel {
  width: 50%;
  padding: 20px;
  background: #1e1e1e;
  overflow: auto;
  color: #ddd;
}

.packet-detail h3, .packet-detail h4, .packet-detail h5 {
  margin-top: 0;
  margin-bottom: 12px;
  color: #fff;
}

.info-grid {
  display: grid;
  grid-template-columns: 100px 1fr;
  gap: 8px;
  margin-bottom: 20px;
}

.info-grid label {
  color: #888;
  font-size: 12px;
}

.info-grid value {
  font-family: 'Courier New', monospace;
  font-size: 12px;
}

.tlv-header {
  display: flex;
  gap: 20px;
  padding: 10px;
  background: #2d2d2d;
  border-radius: 4px;
  margin-bottom: 16px;
}

.tlv-header .valid {
  color: #4CAF50;
}

.tlv-header .invalid {
  color: #f44336;
}

.tlv-tree {
  font-family: 'Courier New', monospace;
  font-size: 12px;
}

.tlv-element {
  display: flex;
  gap: 12px;
  padding: 6px 0;
  border-bottom: 1px solid #333;
}

.tlv-element .tag {
  color: #569cd6;
  min-width: 40px;
}

.tlv-element .tag-name {
  color: #9cdcfe;
  min-width: 150px;
}

.tlv-element .length {
  color: #b5cea8;
}

.tlv-element .value {
  color: #ce9178;
  flex: 1;
}

.validation-errors, .validation-warnings {
  margin-top: 16px;
  padding: 12px;
  border-radius: 4px;
}

.validation-errors {
  background: rgba(244, 67, 54, 0.1);
  border: 1px solid #f44336;
}

.validation-warnings {
  background: rgba(255, 152, 0, 0.1);
  border: 1px solid #ff9800;
}

.validation-errors h5 {
  color: #f44336;
  margin-top: 0;
}

.validation-warnings h5 {
  color: #ff9800;
  margin-top: 0;
}

.validation-errors li, .validation-warnings li {
  margin: 4px 0;
  font-size: 12px;
}

.statistics-panel {
  height: 180px;
  background: #252526;
  border-top: 1px solid #333;
  padding: 10px 20px;
}

.no-selection {
  color: #666;
  text-align: center;
  padding-top: 50px;
}
```

---

## 4. 使用示例

### 4.1 分析pcap文件

```bash
# 安装依赖
pip install scapy

# 分析pcap文件
python tlv_analyzer.py --pcap capture.pcap --output analysis.json

# 实时捕获
sudo python tlv_analyzer.py --interface eth0

# 代理模式
python tlv_analyzer.py --proxy-port 18830 --proxy-target mqtt.automotive.com:8883
```

### 4.2 Web界面

```bash
# 启动后端
python tlv_websocket_server.py

# 启动前端
cd tlv-visualizer-web
npm install
npm start

# 打开 http://localhost:3000
```

---

*可视化工具版本: v1.0.0*  
*支持抓包: pcap文件、实时捕获、TCP代理*