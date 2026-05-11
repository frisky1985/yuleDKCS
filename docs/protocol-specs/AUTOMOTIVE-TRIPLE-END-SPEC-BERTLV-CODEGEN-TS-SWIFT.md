# BER-TLV 代码生成器扩展 - TypeScript & Swift

> **版本**: v1.1.0  
> **扩展语言**: TypeScript, Swift  
> **协议版本**: v1.2.0

---

## 1. TypeScript 支持

### 1.1 生成结果示例

```typescript
// generated/tlv_protocol.ts
// Auto-generated from schema v1.2.0

/**
 * TLV Protocol Version
 */
export const PROTOCOL_VERSION = '1.2.0';

/**
 * TAG Constants
 */
export enum Tag {
  PROTOCOL_VERSION = 0x80,
  MESSAGE_TYPE = 0x81,
  MESSAGE_ID = 0x82,
  TIMESTAMP = 0x83,
  SESSION_ID = 0x84,
  DEVICE_TYPE = 0x85,
  VEHICLE_INFO = 0x40,
  VIN = 0x41,
  VEHICLE_MODEL = 0x42,
  VEHICLE_BRAND = 0x43,
  LICENSE_PLATE = 0x44,
  VEHICLE_STATUS = 0x45,
  IGNITION = 0x46,
  LOCK_STATUS = 0x47,
  MILEAGE = 0x48,
  FUEL_LEVEL = 0x49,
  BATTERY_VOLTAGE = 0x4A,
  ENGINE_TEMP = 0x4B,
  TIRE_PRESSURE = 0x4C,
  DOOR_STATUS = 0x4D,
  WINDOW_STATUS = 0x4E,
  LOCATION = 0x50,
  LATITUDE = 0x51,
  LONGITUDE = 0x52,
  ALTITUDE = 0x53,
  SPEED = 0x54,
  HEADING = 0x55,
  GPS_ACCURACY = 0x56,
  LOCATION_TIMESTAMP = 0x57
}

/**
 * Message Type Enum
 */
export enum MessageType {
  REQUEST = 0x01,
  RESPONSE = 0x02,
  ERROR = 0x03,
  NOTIFICATION = 0x04,
  HEARTBEAT = 0x05,
  HEARTBEAT_ACK = 0x06,
  COMMAND = 0x07,
  COMMAND_ACK = 0x08,
  COMMAND_RESULT = 0x09,
  OTA_REQUEST = 0x0A,
  OTA_RESPONSE = 0x0B,
  SYNC_REQUEST = 0x0C,
  SYNC_RESPONSE = 0x0D,
  VERSION_NEGOTIATION = 0x0E,
  VERSION_NEGOTIATION_ACK = 0x0F
}

/**
 * Device Type Enum
 */
export enum DeviceType {
  TBOX = 0x01,
  IVI = 0x02,
  ADAS = 0x03,
  CLUSTER = 0x04
}

/**
 * Error Codes
 */
export enum ErrorCode {
  SUCCESS = 0x0000,
  INVALID_MESSAGE = 0x0001,
  UNSUPPORTED_VERSION = 0x0002,
  AUTHENTICATION_FAILED = 0x0003,
  AUTHORIZATION_FAILED = 0x0004,
  RATE_LIMIT_EXCEEDED = 0x0005,
  TIMEOUT = 0x0006,
  ENCRYPTION_FAILED = 0x0007,
  COMPRESSION_FAILED = 0x0008,
  INVALID_COMMAND = 0x0100,
  VEHICLE_OFFLINE = 0x0101,
  COMMAND_TIMEOUT = 0x0102,
  OTA_FAILED = 0x0200,
  VERSION_NOT_SUPPORTED = 0x2001
}

/**
 * Protocol Version
 */
export interface ProtocolVersion {
  major: number;
  minor: number;
  patch: number;
}

/**
 * Vehicle Info
 */
export interface VehicleInfo {
  vin: string;
  vehicleModel: string;
  vehicleBrand?: string;
  licensePlate?: string;
}

/**
 * Vehicle Status
 */
export interface VehicleStatus {
  ignition: boolean;
  lockStatus: boolean;
  mileage: bigint;
  fuelLevel: bigint;
  batteryVoltage?: bigint;
  engineTemp?: bigint;
  tirePressure?: TirePressure;
  doorStatus?: DoorStatus;
  windowStatus?: WindowStatus;
}

/**
 * Tire Pressure
 */
export interface TirePressure {
  frontLeft: number;
  frontRight: number;
  rearLeft: number;
  rearRight: number;
}

/**
 * Door Status
 */
export interface DoorStatus {
  frontLeft: boolean;
  frontRight: boolean;
  rearLeft: boolean;
  rearRight: boolean;
  trunk: boolean;
  hood: boolean;
}

/**
 * Window Status
 */
export interface WindowStatus {
  frontLeft: number;
  frontRight: number;
  rearLeft: number;
  rearRight: number;
  sunroof: number;
}

/**
 * Location
 */
export interface Location {
  latitude: bigint;
  longitude: bigint;
  altitude?: bigint;
  speed?: bigint;
  heading?: number;
  gpsAccuracy?: number;
  timestamp?: bigint;
}

/**
 * TLV Message Base
 */
export interface TlvMessage {
  protocolVersion: ProtocolVersion;
  messageType: MessageType;
  messageId: Uint8Array; // 16 bytes UUID
  timestamp: bigint;
  sessionId?: Uint8Array;
}

/**
 * Heartbeat Message
 */
export interface HeartbeatMessage extends TlvMessage {
  uptime?: bigint;
  signalStrength?: number;
  batteryLevel?: number;
}

/**
 * Vehicle Status Message
 */
export interface VehicleStatusMessage extends TlvMessage {
  vehicleStatus: VehicleStatus;
}

/**
 * Location Update Message
 */
export interface LocationUpdateMessage extends TlvMessage {
  location: Location;
}

/**
 * Command Message
 */
export interface CommandMessage extends TlvMessage {
  requestType: number;
  parameters?: Map<number, Uint8Array>;
}

/**
 * Command Result
 */
export interface CommandResult {
  success: boolean;
  resultCode: ErrorCode;
  resultData?: Uint8Array;
}

/**
 * TLV Encoder
 */
export class TlvEncoder {
  private buffer: Uint8Array;
  private offset: number = 0;

  constructor(initialSize: number = 4096) {
    this.buffer = new Uint8Array(initialSize);
  }

  /**
   * Encode protocol version
   */
  encodeVersion(version: ProtocolVersion): this {
    this.writeTag(Tag.PROTOCOL_VERSION);
    this.writeLength(3);
    this.buffer[this.offset++] = version.major;
    this.buffer[this.offset++] = version.minor;
    this.buffer[this.offset++] = version.patch;
    return this;
  }

  /**
   * Encode message type
   */
  encodeMessageType(type: MessageType): this {
    this.writeTag(Tag.MESSAGE_TYPE);
    this.writeLength(1);
    this.buffer[this.offset++] = type;
    return this;
  }

  /**
   * Encode UUID (16 bytes)
   */
  encodeUUID(tag: Tag, uuid: Uint8Array): this {
    if (uuid.length !== 16) {
      throw new Error('UUID must be 16 bytes');
    }
    this.writeTag(tag);
    this.writeLength(16);
    this.buffer.set(uuid, this.offset);
    this.offset += 16;
    return this;
  }

  /**
   * Encode timestamp (8 bytes, milliseconds)
   */
  encodeTimestamp(timestamp: bigint): this {
    this.writeTag(Tag.TIMESTAMP);
    this.writeLength(8);
    const view = new DataView(this.buffer.buffer, this.offset);
    view.setBigUint64(0, timestamp, false); // big-endian
    this.offset += 8;
    return this;
  }

  /**
   * Encode boolean
   */
  encodeBoolean(tag: Tag, value: boolean): this {
    this.writeTag(tag);
    this.writeLength(1);
    this.buffer[this.offset++] = value ? 0xFF : 0x00;
    return this;
  }

  /**
   * Encode integer (variable length)
   */
  encodeInteger(tag: Tag, value: bigint, length: number = 8): this {
    this.writeTag(tag);
    this.writeLength(length);
    
    for (let i = length - 1; i >= 0; i--) {
      this.buffer[this.offset + i] = Number(value & BigInt(0xFF));
      value = value >> BigInt(8);
    }
    this.offset += length;
    return this;
  }

  /**
   * Encode UTF-8 string
   */
  encodeString(tag: Tag, value: string): this {
    const encoder = new TextEncoder();
    const bytes = encoder.encode(value);
    this.writeTag(tag);
    this.writeLength(bytes.length);
    this.buffer.set(bytes, this.offset);
    this.offset += bytes.length;
    return this;
  }

  /**
   * Encode raw bytes
   */
  encodeBytes(tag: Tag, value: Uint8Array): this {
    this.writeTag(tag);
    this.writeLength(value.length);
    this.buffer.set(value, this.offset);
    this.offset += value.length;
    return this;
  }

  /**
   * Encode vehicle info
   */
  encodeVehicleInfo(info: VehicleInfo): this {
    this.writeTag(Tag.VEHICLE_INFO);
    const startOffset = this.offset;
    this.offset += 4; // Reserve space for length

    this.encodeString(Tag.VIN, info.vin);
    this.encodeString(Tag.VEHICLE_MODEL, info.vehicleModel);
    if (info.vehicleBrand) {
      this.encodeString(Tag.VEHICLE_BRAND, info.vehicleBrand);
    }
    if (info.licensePlate) {
      this.encodeString(Tag.LICENSE_PLATE, info.licensePlate);
    }

    // Update length
    const length = this.offset - startOffset - 4;
    const lengthOffset = startOffset;
    this.offset = lengthOffset;
    this.writeLengthBytes(length);
    this.offset += length;

    return this;
  }

  /**
   * Get encoded result
   */
  finish(): Uint8Array {
    return this.buffer.slice(0, this.offset);
  }

  private writeTag(tag: Tag | number): void {
    this.ensureCapacity(1);
    this.buffer[this.offset++] = tag;
  }

  private writeLength(length: number): void {
    this.ensureCapacity(5);
    this.writeLengthBytes(length);
  }

  private writeLengthBytes(length: number): void {
    if (length < 128) {
      this.buffer[this.offset++] = length;
    } else if (length < 256) {
      this.buffer[this.offset++] = 0x81;
      this.buffer[this.offset++] = length;
    } else if (length < 65536) {
      this.buffer[this.offset++] = 0x82;
      this.buffer[this.offset++] = (length >> 8) & 0xFF;
      this.buffer[this.offset++] = length & 0xFF;
    } else {
      this.buffer[this.offset++] = 0x83;
      this.buffer[this.offset++] = (length >> 16) & 0xFF;
      this.buffer[this.offset++] = (length >> 8) & 0xFF;
      this.buffer[this.offset++] = length & 0xFF;
    }
  }

  private ensureCapacity(required: number): void {
    if (this.offset + required > this.buffer.length) {
      const newBuffer = new Uint8Array(this.buffer.length * 2);
      newBuffer.set(this.buffer);
      this.buffer = newBuffer;
    }
  }
}

/**
 * TLV Decoder
 */
export class TlvDecoder {
  private buffer: Uint8Array;
  private offset: number = 0;

  constructor(buffer: Uint8Array) {
    this.buffer = buffer;
  }

  /**
   * Decode all elements
   */
  *decodeAll(): Generator<TlvElement> {
    while (this.offset < this.buffer.length) {
      yield this.decodeNext();
    }
  }

  /**
   * Decode next element
   */
  decodeNext(): TlvElement {
    if (this.offset >= this.buffer.length) {
      throw new Error('Unexpected end of buffer');
    }

    const tag = this.buffer[this.offset++];
    const length = this.decodeLength();
    
    if (this.offset + length > this.buffer.length) {
      throw new Error(`Length ${length} exceeds buffer`);
    }

    const value = this.buffer.slice(this.offset, this.offset + length);
    this.offset += length;

    return { tag, length, value, offset: this.offset - length - 2 };
  }

  /**
   * Find element by tag
   */
  findElement(tag: Tag): TlvElement | undefined {
    const savedOffset = this.offset;
    this.offset = 0;

    for (const elem of this.decodeAll()) {
      if (elem.tag === tag) {
        this.offset = savedOffset;
        return elem;
      }
    }

    this.offset = savedOffset;
    return undefined;
  }

  /**
   * Decode protocol version
   */
  decodeVersion(): ProtocolVersion {
    const elem = this.findElement(Tag.PROTOCOL_VERSION);
    if (!elem || elem.length !== 3) {
      throw new Error('Invalid or missing protocol version');
    }
    return {
      major: elem.value[0],
      minor: elem.value[1],
      patch: elem.value[2]
    };
  }

  /**
   * Decode message type
   */
  decodeMessageType(): MessageType {
    const elem = this.findElement(Tag.MESSAGE_TYPE);
    if (!elem || elem.length !== 1) {
      throw new Error('Invalid or missing message type');
    }
    return elem.value[0] as MessageType;
  }

  /**
   * Decode UUID
   */
  decodeUUID(tag: Tag): Uint8Array {
    const elem = this.findElement(tag);
    if (!elem || elem.length !== 16) {
      throw new Error('Invalid or missing UUID');
    }
    return elem.value;
  }

  /**
   * Decode timestamp
   */
  decodeTimestamp(): bigint {
    const elem = this.findElement(Tag.TIMESTAMP);
    if (!elem || elem.length !== 8) {
      throw new Error('Invalid or missing timestamp');
    }
    const view = new DataView(elem.value.buffer);
    return view.getBigUint64(0, false);
  }

  /**
   * Decode string
   */
  decodeString(tag: Tag): string {
    const elem = this.findElement(tag);
    if (!elem) {
      throw new Error(`Missing tag: ${Tag[tag]}`);
    }
    const decoder = new TextDecoder();
    return decoder.decode(elem.value);
  }

  /**
   * Decode integer
   */
  decodeInteger(tag: Tag, signed: boolean = false): bigint {
    const elem = this.findElement(tag);
    if (!elem) {
      throw new Error(`Missing tag: ${Tag[tag]}`);
    }

    let result = BigInt(0);
    for (let i = 0; i < elem.value.length; i++) {
      result = (result << BigInt(8)) | BigInt(elem.value[i]);
    }

    if (signed && (elem.value[0] & 0x80)) {
      const bits = BigInt(elem.value.length * 8);
      result = result - (BigInt(1) << bits);
    }

    return result;
  }

  /**
   * Decode boolean
   */
  decodeBoolean(tag: Tag): boolean {
    const elem = this.findElement(tag);
    if (!elem || elem.length !== 1) {
      throw new Error(`Invalid boolean tag: ${Tag[tag]}`);
    }
    return elem.value[0] !== 0x00;
  }

  /**
   * Validate message
   */
  validate(): ValidationResult {
    const errors: ValidationError[] = [];
    const warnings: ValidationError[] = [];

    try {
      // Check required tags
      const requiredTags = [Tag.PROTOCOL_VERSION, Tag.MESSAGE_TYPE, Tag.MESSAGE_ID, Tag.TIMESTAMP];
      for (const tag of requiredTags) {
        if (!this.findElement(tag)) {
          errors.push({
            code: 'MISSING_REQUIRED_TAG',
            message: `Missing required tag: ${Tag[tag]}`,
            tag
          });
        }
      }

      // Validate version
      try {
        const version = this.decodeVersion();
        if (version.major !== 1 || version.minor !== 2) {
          warnings.push({
            code: 'UNSUPPORTED_VERSION',
            message: `Version ${version.major}.${version.minor}.${version.patch} may not be fully supported`,
            tag: Tag.PROTOCOL_VERSION
          });
        }
      } catch (e) {
        errors.push({
          code: 'INVALID_VERSION',
          message: 'Failed to decode protocol version',
          tag: Tag.PROTOCOL_VERSION
        });
      }

      // Validate message type
      try {
        const msgType = this.decodeMessageType();
        if (!Object.values(MessageType).includes(msgType)) {
          warnings.push({
            code: 'UNKNOWN_MESSAGE_TYPE',
            message: `Unknown message type: ${msgType}`,
            tag: Tag.MESSAGE_TYPE
          });
        }
      } catch (e) {
        errors.push({
          code: 'INVALID_MESSAGE_TYPE',
          message: 'Failed to decode message type',
          tag: Tag.MESSAGE_TYPE
        });
      }

    } catch (e) {
      errors.push({
        code: 'PARSE_ERROR',
        message: e instanceof Error ? e.message : 'Unknown error'
      });
    }

    return {
      valid: errors.length === 0,
      errors,
      warnings
    };
  }

  private decodeLength(): number {
    if (this.offset >= this.buffer.length) {
      throw new Error('Unexpected end of buffer');
    }

    const first = this.buffer[this.offset++];

    if ((first & 0x80) === 0) {
      return first;
    }

    const numBytes = first & 0x7F;
    if (numBytes === 0) {
      throw new Error('Indefinite length not supported');
    }
    if (numBytes > 4) {
      throw new Error('Length too large');
    }
    if (this.offset + numBytes > this.buffer.length) {
      throw new Error('Length bytes exceed buffer');
    }

    let length = 0;
    for (let i = 0; i < numBytes; i++) {
      length = (length << 8) | this.buffer[this.offset++];
    }

    return length;
  }
}

/**
 * TLV Element
 */
export interface TlvElement {
  tag: number;
  length: number;
  value: Uint8Array;
  offset: number;
}

/**
 * Validation Error
 */
export interface ValidationError {
  code: string;
  message: string;
  tag?: Tag;
}

/**
 * Validation Result
 */
export interface ValidationResult {
  valid: boolean;
  errors: ValidationError[];
  warnings: ValidationError[];
}

/**
 * Create heartbeat message
 */
export function createHeartbeat(
  messageId: Uint8Array,
  options?: {
    uptime?: bigint;
    signalStrength?: number;
    batteryLevel?: number;
  }
): Uint8Array {
  const encoder = new TlvEncoder();
  
  encoder
    .encodeVersion({ major: 1, minor: 2, patch: 0 })
    .encodeMessageType(MessageType.HEARTBEAT)
    .encodeUUID(Tag.MESSAGE_ID, messageId)
    .encodeTimestamp(BigInt(Date.now()));

  if (options?.uptime !== undefined) {
    encoder.encodeInteger(Tag.UPTIME, options.uptime, 8);
  }
  if (options?.signalStrength !== undefined) {
    encoder.encodeInteger(Tag.SIGNAL_STRENGTH, BigInt(options.signalStrength), 1);
  }
  if (options?.batteryLevel !== undefined) {
    encoder.encodeInteger(Tag.BATTERY_LEVEL, BigInt(options.batteryLevel), 1);
  }

  return encoder.finish();
}

/**
 * Parse heartbeat message
 */
export function parseHeartbeat(data: Uint8Array): HeartbeatMessage {
  const decoder = new TlvDecoder(data);
  const validation = decoder.validate();
  
  if (!validation.valid) {
    throw new Error(`Validation failed: ${validation.errors.map(e => e.message).join(', ')}`);
  }

  const msg: HeartbeatMessage = {
    protocolVersion: decoder.decodeVersion(),
    messageType: decoder.decodeMessageType(),
    messageId: decoder.decodeUUID(Tag.MESSAGE_ID),
    timestamp: decoder.decodeTimestamp()
  };

  // Optional fields
  try {
    msg.uptime = decoder.decodeInteger(Tag.UPTIME);
  } catch { /* optional */ }

  try {
    msg.signalStrength = Number(decoder.decodeInteger(Tag.SIGNAL_STRENGTH));
  } catch { /* optional */ }

  try {
    msg.batteryLevel = Number(decoder.decodeInteger(Tag.BATTERY_LEVEL));
  } catch { /* optional */ }

  return msg;
}

// Additional tags for optional fields
export enum OptionalTag {
  UPTIME = 0x60,
  SIGNAL_STRENGTH = 0x61,
  BATTERY_LEVEL = 0x62
}
```

### 1.2 使用示例

```typescript
// 使用示例
import { TlvEncoder, TlvDecoder, MessageType, createHeartbeat, parseHeartbeat } from './tlv_protocol';

// 编码消息
const encoder = new TlvEncoder();
const message = encoder
  .encodeVersion({ major: 1, minor: 2, patch: 0 })
  .encodeMessageType(MessageType.HEARTBEAT)
  .encodeUUID(Tag.MESSAGE_ID, crypto.getRandomValues(new Uint8Array(16)))
  .encodeTimestamp(BigInt(Date.now()))
  .encodeBoolean(Tag.IGNITION, true)
  .finish();

// 发送到MQTT
mqttClient.publish('vehicle/heartbeat', message);

// 解码消息
const decoder = new TlvDecoder(receivedData);
const validation = decoder.validate();

if (validation.valid) {
  const version = decoder.decodeVersion();
  console.log(`Protocol version: ${version.major}.${version.minor}.${version.patch}`);
  
  const msgType = decoder.decodeMessageType();
  console.log(`Message type: ${MessageType[msgType]}`);
}

// 使用高级API
const heartbeat = createHeartbeat(crypto.getRandomValues(new Uint8Array(16)), {
  uptime: BigInt(3600000),
  signalStrength: 85,
  batteryLevel: 75
});

const parsed = parseHeartbeat(heartbeat);
console.log(parsed);
```

---

## 2. Swift 支持

### 2.1 生成结果示例

```swift
// TlvProtocol.swift
// Auto-generated from schema v1.2.0

import Foundation

// MARK: - Constants

public let PROTOCOL_VERSION = "1.2.0"

// MARK: - TAG Enum

public enum Tag: UInt8 {
    case protocolVersion = 0x80
    case messageType = 0x81
    case messageId = 0x82
    case timestamp = 0x83
    case sessionId = 0x84
    case deviceType = 0x85
    case vehicleInfo = 0x40
    case vin = 0x41
    case vehicleModel = 0x42
    case vehicleBrand = 0x43
    case licensePlate = 0x44
    case vehicleStatus = 0x45
    case ignition = 0x46
    case lockStatus = 0x47
    case mileage = 0x48
    case fuelLevel = 0x49
    case batteryVoltage = 0x4A
    case engineTemp = 0x4B
    case tirePressure = 0x4C
    case doorStatus = 0x4D
    case windowStatus = 0x4E
    case location = 0x50
    case latitude = 0x51
    case longitude = 0x52
    case altitude = 0x53
    case speed = 0x54
    case heading = 0x55
    case gpsAccuracy = 0x56
    case locationTimestamp = 0x57
}

// MARK: - Message Type Enum

public enum MessageType: UInt8 {
    case request = 0x01
    case response = 0x02
    case error = 0x03
    case notification = 0x04
    case heartbeat = 0x05
    case heartbeatAck = 0x06
    case command = 0x07
    case commandAck = 0x08
    case commandResult = 0x09
    case otaRequest = 0x0A
    case otaResponse = 0x0B
    case syncRequest = 0x0C
    case syncResponse = 0x0D
    case versionNegotiation = 0x0E
    case versionNegotiationAck = 0x0F
}

// MARK: - Device Type Enum

public enum DeviceType: UInt8 {
    case tbox = 0x01
    case ivi = 0x02
    case adas = 0x03
    case cluster = 0x04
}

// MARK: - Error Codes

public enum ErrorCode: UInt16 {
    case success = 0x0000
    case invalidMessage = 0x0001
    case unsupportedVersion = 0x0002
    case authenticationFailed = 0x0003
    case authorizationFailed = 0x0004
    case rateLimitExceeded = 0x0005
    case timeout = 0x0006
    case encryptionFailed = 0x0007
    case compressionFailed = 0x0008
    case invalidCommand = 0x0100
    case vehicleOffline = 0x0101
    case commandTimeout = 0x0102
    case otaFailed = 0x0200
    case versionNotSupported = 0x2001
}

// MARK: - Protocol Errors

public enum TlvError: Error {
    case bufferOverflow
    case invalidTag
    case invalidLength
    case bufferTooSmall
    case truncatedData
    case missingRequiredField(Tag)
    case invalidValue(String)
    case parseError(String)
}

// MARK: - Data Structures

public struct ProtocolVersion {
    public let major: UInt8
    public let minor: UInt8
    public let patch: UInt8
    
    public init(major: UInt8, minor: UInt8, patch: UInt8) {
        self.major = major
        self.minor = minor
        self.patch = patch
    }
    
    public var stringValue: String {
        return "\(major).\(minor).\(patch)"
    }
}

public struct VehicleInfo {
    public let vin: String
    public let vehicleModel: String
    public var vehicleBrand: String?
    public var licensePlate: String?
    
    public init(vin: String, vehicleModel: String, vehicleBrand: String? = nil, licensePlate: String? = nil) {
        self.vin = vin
        self.vehicleModel = vehicleModel
        self.vehicleBrand = vehicleBrand
        self.licensePlate = licensePlate
    }
}

public struct VehicleStatus {
    public var ignition: Bool
    public var lockStatus: Bool
    public var mileage: UInt64
    public var fuelLevel: UInt64
    public var batteryVoltage: UInt64?
    public var engineTemp: UInt64?
    public var tirePressure: TirePressure?
    public var doorStatus: DoorStatus?
    public var windowStatus: WindowStatus?
}

public struct TirePressure {
    public var frontLeft: Float
    public var frontRight: Float
    public var rearLeft: Float
    public var rearRight: Float
}

public struct DoorStatus {
    public var frontLeft: Bool
    public var frontRight: Bool
    public var rearLeft: Bool
    public var rearRight: Bool
    public var trunk: Bool
    public var hood: Bool
}

public struct WindowStatus {
    public var frontLeft: UInt8
    public var frontRight: UInt8
    public var rearLeft: UInt8
    public var rearRight: UInt8
    public var sunroof: UInt8
}

public struct Location {
    public var latitude: Int64
    public var longitude: Int64
    public var altitude: Int64?
    public var speed: UInt64?
    public var heading: UInt16?
    public var gpsAccuracy: UInt16?
    public var timestamp: UInt64?
}

// MARK: - TLV Message Protocol

public protocol TlvMessage {
    var protocolVersion: ProtocolVersion { get }
    var messageType: MessageType { get }
    var messageId: UUID { get }
    var timestamp: UInt64 { get }
    var sessionId: UUID? { get }
}

public struct HeartbeatMessage: TlvMessage {
    public let protocolVersion: ProtocolVersion
    public let messageType: MessageType = .heartbeat
    public let messageId: UUID
    public let timestamp: UInt64
    public var sessionId: UUID?
    public var uptime: UInt64?
    public var signalStrength: UInt8?
    public var batteryLevel: UInt8?
}

// MARK: - TLV Encoder

public class TlvEncoder {
    private var buffer: [UInt8]
    private var offset: Int = 0
    
    public init(initialCapacity: Int = 4096) {
        self.buffer = [UInt8](repeating: 0, count: initialCapacity)
    }
    
    @discardableResult
    public func encodeVersion(_ version: ProtocolVersion) -> Self {
        writeTag(.protocolVersion)
        writeLength(3)
        buffer[offset] = version.major
        buffer[offset + 1] = version.minor
        buffer[offset + 2] = version.patch
        offset += 3
        return self
    }
    
    @discardableResult
    public func encodeMessageType(_ type: MessageType) -> Self {
        writeTag(.messageType)
        writeLength(1)
        buffer[offset] = type.rawValue
        offset += 1
        return self
    }
    
    @discardableResult
    public func encodeUUID(_ tag: Tag, _ uuid: UUID) -> Self {
        writeTag(tag)
        writeLength(16)
        var uuidBytes = uuid.uuid
        withUnsafeBytes(of: &uuidBytes) { ptr in
            memcpy(&buffer[offset], ptr.baseAddress!, 16)
        }
        offset += 16
        return self
    }
    
    @discardableResult
    public func encodeTimestamp(_ timestamp: UInt64) -> Self {
        writeTag(.timestamp)
        writeLength(8)
        var bigEndian = timestamp.bigEndian
        withUnsafeBytes(of: &bigEndian) { ptr in
            memcpy(&buffer[offset], ptr.baseAddress!, 8)
        }
        offset += 8
        return self
    }
    
    @discardableResult
    public func encodeBoolean(_ tag: Tag, _ value: Bool) -> Self {
        writeTag(tag)
        writeLength(1)
        buffer[offset] = value ? 0xFF : 0x00
        offset += 1
        return self
    }
    
    @discardableResult
    public func encodeInteger(_ tag: Tag, _ value: UInt64, length: Int = 8) -> Self {
        writeTag(tag)
        writeLength(length)
        var temp = value
        for i in (0..<length).reversed() {
            buffer[offset + i] = UInt8(temp & 0xFF)
            temp >>= 8
        }
        offset += length
        return self
    }
    
    @discardableResult
    public func encodeString(_ tag: Tag, _ value: String) -> Self {
        let data = value.data(using: .utf8)!
        writeTag(tag)
        writeLength(data.count)
        data.copyBytes(to: &buffer[offset..<offset+data.count], from: 0..<data.count)
        offset += data.count
        return self
    }
    
    @discardableResult
    public func encodeBytes(_ tag: Tag, _ value: [UInt8]) -> Self {
        writeTag(tag)
        writeLength(value.count)
        buffer[offset..<offset+value.count] = value[0..<value.count]
        offset += value.count
        return self
    }
    
    @discardableResult
    public func encodeVehicleInfo(_ info: VehicleInfo) -> Self {
        writeTag(.vehicleInfo)
        let startOffset = offset
        offset += 4 // Reserve for length
        
        encodeString(.vin, info.vin)
        encodeString(.vehicleModel, info.vehicleModel)
        if let brand = info.vehicleBrand {
            encodeString(.vehicleBrand, brand)
        }
        if let plate = info.licensePlate {
            encodeString(.licensePlate, plate)
        }
        
        // Update length
        let length = offset - startOffset - 4
        var lengthOffset = startOffset
        writeLengthAt(&lengthOffset, length)
        
        return self
    }
    
    public func finish() -> [UInt8] {
        return Array(buffer[0..<offset])
    }
    
    // MARK: - Private Methods
    
    private func writeTag(_ tag: Tag) {
        ensureCapacity(1)
        buffer[offset] = tag.rawValue
        offset += 1
    }
    
    private func writeLength(_ length: Int) {
        ensureCapacity(5)
        if length < 128 {
            buffer[offset] = UInt8(length)
            offset += 1
        } else if length < 256 {
            buffer[offset] = 0x81
            buffer[offset + 1] = UInt8(length)
            offset += 2
        } else if length < 65536 {
            buffer[offset] = 0x82
            buffer[offset + 1] = UInt8(length >> 8)
            buffer[offset + 2] = UInt8(length & 0xFF)
            offset += 3
        } else {
            buffer[offset] = 0x83
            buffer[offset + 1] = UInt8((length >> 16) & 0xFF)
            buffer[offset + 2] = UInt8((length >> 8) & 0xFF)
            buffer[offset + 3] = UInt8(length & 0xFF)
            offset += 4
        }
    }
    
    private func writeLengthAt(_ offsetPtr: inout Int, _ length: Int) {
        if length < 128 {
            buffer[offsetPtr] = UInt8(length)
            offsetPtr += 1
        } else if length < 256 {
            buffer[offsetPtr] = 0x81
            buffer[offsetPtr + 1] = UInt8(length)
            offsetPtr += 2
        } else {
            buffer[offsetPtr] = 0x82
            buffer[offsetPtr + 1] = UInt8(length >> 8)
            buffer[offsetPtr + 2] = UInt8(length & 0xFF)
            offsetPtr += 3
        }
    }
    
    private func ensureCapacity(_ required: Int) {
        if offset + required > buffer.count {
            buffer.append(contentsOf: [UInt8](repeating: 0, count: buffer.count))
        }
    }
}

// MARK: - TLV Decoder

public class TlvDecoder {
    private let buffer: [UInt8]
    private var offset: Int = 0
    private var savedOffset: Int = 0
    
    public init(_ data: [UInt8]) {
        self.buffer = data
    }
    
    public init(_ data: Data) {
        self.buffer = [UInt8](data)
    }
    
    // MARK: - Iterator Protocol
    
    public func nextElement() throws -> TlvElement? {
        guard offset < buffer.count else { return nil }
        
        let tag = try readTag()
        let length = try readLength()
        
        guard offset + length <= buffer.count else {
            throw TlvError.truncatedData
        }
        
        let value = Array(buffer[offset..<offset+length])
        let element = TlvElement(tag: tag, length: length, value: value, offset: offset - 2)
        offset += length
        
        return element
    }
    
    // MARK: - Find Methods
    
    public func findElement(_ tag: Tag) -> TlvElement? {
        savedOffset = offset
        offset = 0
        
        while let element = try? nextElement() {
            if element.tag == tag {
                offset = savedOffset
                return element
            }
        }
        
        offset = savedOffset
        return nil
    }
    
    // MARK: - Decode Methods
    
    public func decodeVersion() throws -> ProtocolVersion {
        guard let elem = findElement(.protocolVersion), elem.length == 3 else {
            throw TlvError.missingRequiredField(.protocolVersion)
        }
        return ProtocolVersion(
            major: elem.value[0],
            minor: elem.value[1],
            patch: elem.value[2]
        )
    }
    
    public func decodeMessageType() throws -> MessageType {
        guard let elem = findElement(.messageType), elem.length == 1,
              let type = MessageType(rawValue: elem.value[0]) else {
            throw TlvError.missingRequiredField(.messageType)
        }
        return type
    }
    
    public func decodeUUID(_ tag: Tag) throws -> UUID {
        guard let elem = findElement(tag), elem.length == 16 else {
            throw TlvError.missingRequiredField(tag)
        }
        return UUID(uuid: (elem.value[0], elem.value[1], elem.value[2], elem.value[3],
                          elem.value[4], elem.value[5], elem.value[6], elem.value[7],
                          elem.value[8], elem.value[9], elem.value[10], elem.value[11],
                          elem.value[12], elem.value[13], elem.value[14], elem.value[15]))
    }
    
    public func decodeTimestamp() throws -> UInt64 {
        guard let elem = findElement(.timestamp), elem.length == 8 else {
            throw TlvError.missingRequiredField(.timestamp)
        }
        return UInt64(bigEndian: Data(elem.value).withUnsafeBytes { $0.load(as: UInt64.self) })
    }
    
    public func decodeString(_ tag: Tag) throws -> String {
        guard let elem = findElement(tag),
              let str = String(bytes: elem.value, encoding: .utf8) else {
            throw TlvError.invalidValue("Cannot decode string for tag \(tag)")
        }
        return str
    }
    
    public func decodeInteger(_ tag: Tag) throws -> UInt64 {
        guard let elem = findElement(tag) else {
            throw TlvError.missingRequiredField(tag)
        }
        var result: UInt64 = 0
        for byte in elem.value {
            result = (result << 8) | UInt64(byte)
        }
        return result
    }
    
    public func decodeBoolean(_ tag: Tag) throws -> Bool {
        guard let elem = findElement(tag), elem.length == 1 else {
            throw TlvError.missingRequiredField(tag)
        }
        return elem.value[0] != 0x00
    }
    
    // MARK: - Validation
    
    public func validate() -> ValidationResult {
        var errors: [ValidationError] = []
        var warnings: [ValidationError] = []
        
        // Check required tags
        let requiredTags: [Tag] = [.protocolVersion, .messageType, .messageId, .timestamp]
        for tag in requiredTags {
            if findElement(tag) == nil {
                errors.append(ValidationError(
                    code: "MISSING_REQUIRED_TAG",
                    message: "Missing required tag: \(tag)",
                    tag: tag
                ))
            }
        }
        
        // Validate version
        do {
            let version = try decodeVersion()
            if version.major != 1 || version.minor != 2 {
                warnings.append(ValidationError(
                    code: "UNSUPPORTED_VERSION",
                    message: "Version \(version.stringValue) may not be fully supported",
                    tag: .protocolVersion
                ))
            }
        } catch {
            errors.append(ValidationError(
                code: "INVALID_VERSION",
                message: "Failed to decode protocol version",
                tag: .protocolVersion
            ))
        }
        
        return ValidationResult(valid: errors.isEmpty, errors: errors, warnings: warnings)
    }
    
    // MARK: - Private Methods
    
    private func readTag() throws -> Tag {
        guard offset < buffer.count else {
            throw TlvError.truncatedData
        }
        guard let tag = Tag(rawValue: buffer[offset]) else {
            throw TlvError.invalidTag
        }
        offset += 1
        return tag
    }
    
    private func readLength() throws -> Int {
        guard offset < buffer.count else {
            throw TlvError.truncatedData
        }
        
        let first = buffer[offset]
        offset += 1
        
        if (first & 0x80) == 0 {
            return Int(first)
        }
        
        let numBytes = Int(first & 0x7F)
        guard numBytes != 0 else {
            throw TlvError.invalidLength
        }
        guard numBytes <= 4 else {
            throw TlvError.invalidLength
        }
        guard offset + numBytes <= buffer.count else {
            throw TlvError.truncatedData
        }
        
        var length = 0
        for _ in 0..<numBytes {
            length = (length << 8) | Int(buffer[offset])
            offset += 1
        }
        
        return length
    }
}

// MARK: - Supporting Types

public struct TlvElement {
    public let tag: Tag
    public let length: Int
    public let value: [UInt8]
    public let offset: Int
}

public struct ValidationError {
    public let code: String
    public let message: String
    public let tag: Tag?
}

public struct ValidationResult {
    public let valid: Bool
    public let errors: [ValidationError]
    public let warnings: [ValidationError]
}

// MARK: - Convenience Functions

public func createHeartbeat(
    messageId: UUID,
    uptime: UInt64? = nil,
    signalStrength: UInt8? = nil,
    batteryLevel: UInt8? = nil
) -> [UInt8] {
    let encoder = TlvEncoder()
    
    encoder
        .encodeVersion(ProtocolVersion(major: 1, minor: 2, patch: 0))
        .encodeMessageType(.heartbeat)
        .encodeUUID(.messageId, messageId)
        .encodeTimestamp(UInt64(Date().timeIntervalSince1970 * 1000))
    
    if let uptime = uptime {
        encoder.encodeInteger(Tag(rawValue: 0x60)!, uptime, length: 8)
    }
    if let signal = signalStrength {
        encoder.encodeInteger(Tag(rawValue: 0x61)!, UInt64(signal), length: 1)
    }
    if let battery = batteryLevel {
        encoder.encodeInteger(Tag(rawValue: 0x62)!, UInt64(battery), length: 1)
    }
    
    return encoder.finish()
}

public func parseHeartbeat(_ data: [UInt8]) throws -> HeartbeatMessage {
    let decoder = TlvDecoder(data)
    let validation = decoder.validate()
    
    guard validation.valid else {
        let errorMessages = validation.errors.map { $0.message }.joined(separator: ", ")
        throw TlvError.parseError("Validation failed: \(errorMessages)")
    }
    
    var message = HeartbeatMessage(
        protocolVersion: try decoder.decodeVersion(),
        messageId: try decoder.decodeUUID(.messageId),
        timestamp: try decoder.decodeTimestamp()
    )
    
    // Optional fields
    if let uptime = try? decoder.decodeInteger(Tag(rawValue: 0x60)!) {
        message.uptime = uptime
    }
    if let signal = try? decoder.decodeInteger(Tag(rawValue: 0x61)!) {
        message.signalStrength = UInt8(signal)
    }
    if let battery = try? decoder.decodeInteger(Tag(rawValue: 0x62)!) {
        message.batteryLevel = UInt8(battery)
    }
    
    return message
}
```

### 2.2 SwiftUI 示例

```swift
import SwiftUI

struct TlvDebuggerView: View {
    @State private var hexInput = ""
    @State private var decodedElements: [DecodedElement] = []
    @State private var errorMessage: String?
    
    var body: some View {
        VStack(spacing: 16) {
            Text("TLV Message Debugger")
                .font(.title)
            
            TextEditor(text: $hexInput)
                .font(.system(.body, design: .monospaced))
                .frame(height: 100)
                .border(Color.gray, width: 1)
                .overlay(
                    HStack {
                        Spacer()
                        VStack {
                            Spacer()
                            Button("Paste") {
                                if let string = UIPasteboard.general.string {
                                    hexInput = string
                                }
                            }
                            .padding(8)
                        }
                    }
                )
            
            HStack {
                Button("Decode") {
                    decodeInput()
                }
                .buttonStyle(.borderedProminent)
                
                Button("Clear") {
                    hexInput = ""
                    decodedElements = []
                    errorMessage = nil
                }
                .buttonStyle(.bordered)
            }
            
            if let error = errorMessage {
                Text(error)
                    .foregroundColor(.red)
                    .padding()
                    .background(Color.red.opacity(0.1))
                    .cornerRadius(8)
            }
            
            List(decodedElements) { element in
                VStack(alignment: .leading, spacing: 4) {
                    HStack {
                        Text("TAG: 0x\(String(element.tag, radix: 16, uppercase: true).padding(toLength: 2, withPad: "0", startingAt: 0))")
                            .font(.system(.body, design: .monospaced))
                            .bold()
                        Spacer()
                        Text(Tag(rawValue: element.tag)?.description ?? "Unknown")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    
                    Text("Length: \(element.length)")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    Text("Value: \(element.value.prefix(20).map { String(format: "%02X", $0) }.joined(separator: " "))")
                        .font(.system(.caption, design: .monospaced))
                        .foregroundColor(.blue)
                }
                .padding(.vertical, 4)
            }
        }
        .padding()
    }
    
    private func decodeInput() {
        errorMessage = nil
        decodedElements = []
        
        let hexString = hexInput.replacingOccurrences(of: " ", with: "")
            .replacingOccurrences(of: "-", with: "")
        
        guard hexString.count % 2 == 0 else {
            errorMessage = "Invalid hex string length"
            return
        }
        
        var bytes: [UInt8] = []
        var index = hexString.startIndex
        while index < hexString.endIndex {
            let nextIndex = hexString.index(index, offsetBy: 2)
            if let byte = UInt8(hexString[index..<nextIndex], radix: 16) {
                bytes.append(byte)
            }
            index = nextIndex
        }
        
        let decoder = TlvDecoder(bytes)
        
        do {
            while let element = try decoder.nextElement() {
                decodedElements.append(DecodedElement(
                    tag: element.tag.rawValue,
                    length: element.length,
                    value: element.value
                ))
            }
        } catch {
            errorMessage = "Decode error: \(error)"
        }
    }
}

struct DecodedElement: Identifiable {
    let id = UUID()
    let tag: UInt8
    let length: Int
    let value: [UInt8]
}
```

---

## 3. 更新代码生成器

```python
# tlv_codegen_v2.py - 扩展版代码生成器

class TlvCodeGeneratorV2(TlvCodeGenerator):
    """扩展版代码生成器，支持TypeScript和Swift"""
    
    TYPE_MAPPINGS = {
        **TlvCodeGenerator.TYPE_MAPPINGS,
        'typescript': {
            'bool': 'boolean',
            'int': 'bigint',
            'string': 'string',
            'bytes': 'Uint8Array',
            'version': 'ProtocolVersion',
            'enum': 'number'
        },
        'swift': {
            'bool': 'Bool',
            'int': 'UInt64',
            'string': 'String',
            'bytes': '[UInt8]',
            'version': 'ProtocolVersion',
            'enum': 'UInt8'
        }
    }
    
    def generate_typescript(self, output_dir: str):
        """生成TypeScript代码"""
        template = Template(TYPESCRIPT_TEMPLATE)  # 使用上面的模板
        data = self._prepare_template_data('typescript')
        output = template.render(**data)
        
        output_path = Path(output_dir) / 'tlv_protocol.ts'
        output_path.write_text(output)
        print(f"Generated TypeScript: {output_path}")
    
    def generate_swift(self, output_dir: str):
        """生成Swift代码"""
        template = Template(SWIFT_TEMPLATE)  # 使用上面的模板
        data = self._prepare_template_data('swift')
        output = template.render(**data)
        
        output_path = Path(output_dir) / 'TlvProtocol.swift'
        output_path.write_text(output)
        print(f"Generated Swift: {output_path}")
```

---

*扩展版本: v1.1.0*  
*新增语言: TypeScript, Swift*
