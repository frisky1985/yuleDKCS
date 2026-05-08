package unified

import (
	"fmt"
	
	pb "github.com/digitalkey/hub/api/v1"
)

// DeviceInfo 设备信息 (用于设备发现和能力协商)
type DeviceInfo struct {
	DeviceID    string
	Vendor      string        // 厂商: apple/samsung/xiaomi/oppo/vivo/huawei
	Model       string        // 机型
	OS          string        // ios/android
	OSVersion   string
	AppVersion  string
	
	// 协议能力
	Capabilities *CapabilitySet
	
	// 硬件标识
	BLEMAC string
	UWBID  string
	NFCUID string
	SEID   string
	
	// 安全等级
	SecurityLevel int  // 1-5, 根据 SE 芯片等级
	
	Timestamp int64
}

// ToUnifiedDevice 从 protobuf BindKeyRequest 构建设备信息
func FromBindRequest(req *pb.BindKeyRequest) *Device {
	return &Device{
		DeviceID:   req.DeviceId,
		Vendor:    vendorToString(req.Vendor),
		Protocol:  ProtocolFromProto(req.Protocol),
		Capabilities: &CapabilitySet{
			BLE: true,  // 默认可用
			UWB: true,
			NFC: true,
			SE:  true,
		},
	}
}

// ToBindRequest 将统一设备转换为 BindKeyRequest
func (d *Device) ToBindRequest() *pb.BindKeyRequest {
	return &pb.BindKeyRequest{
		DeviceId: d.DeviceID,
		Vendor:   vendorFromString(d.Vendor),
		Protocol: d.Protocol.ToProto(),
	}
}

// FromDeviceInfo 从 DeviceInfo 创建设备
func FromDeviceInfo(info *DeviceInfo) *Device {
	return &Device{
		DeviceID:   info.DeviceID,
		Vendor:    info.Vendor,
		OS:        info.OS,
		Capabilities: info.Capabilities,
		BLEMAC:    info.BLEMAC,
		UWBID:     info.UWBID,
		NFCUID:    info.NFCUID,
		SEID:      info.SEID,
	}
}

// ToDeviceInfo 转换为 DeviceInfo
func (d *Device) ToDeviceInfo() *DeviceInfo {
	return &DeviceInfo{
		DeviceID:   d.DeviceID,
		Vendor:    d.Vendor,
		OS:        d.OS,
		Capabilities: d.Capabilities,
		BLEMAC:    d.BLEMAC,
		UWBID:     d.UWBID,
		NFCUID:    d.NFCUID,
		SEID:      d.SEID,
	}
}

// Match 检查设备是否满足协议要求
func (d *Device) Match(spec *ProtocolSpec) (bool, []string) {
	if d.Capabilities == nil {
		return false, []string{"no capabilities"}
	}
	
	missing := []string{}
	
	if spec.RequiredCaps.BLE && !d.Capabilities.BLE {
		missing = append(missing, "BLE")
	}
	if spec.RequiredCaps.UWB && !d.Capabilities.UWB {
		missing = append(missing, "UWB")
	}
	if spec.RequiredCaps.NFC && !d.Capabilities.NFC {
		missing = append(missing, "NFC")
	}
	if spec.RequiredCaps.SE && !d.Capabilities.SE {
		missing = append(missing, "SE")
	}
	if spec.RequiredCaps.FiRa && !d.Capabilities.FiRa {
		missing = append(missing, "FiRa")
	}
	
	return len(missing) == 0, missing
}

// ---- Vendor conversions ----

func vendorToString(v pb.PhoneVendor) string {
	switch v {
	case pb.PhoneVendor_APPLE:
		return "apple"
	case pb.PhoneVendor_SAMSUNG:
		return "samsung"
	case pb.PhoneVendor_XIAOMI:
		return "xiaomi"
	case pb.PhoneVendor_OPPO:
		return "oppo"
	case pb.PhoneVendor_VIVO:
		return "vivo"
	case pb.PhoneVendor_HUAWEI:
		return "huawei"
	default:
		return "unknown"
	}
}

func vendorFromString(s string) pb.PhoneVendor {
	switch s {
	case "apple":
		return pb.PhoneVendor_APPLE
	case "samsung":
		return pb.PhoneVendor_SAMSUNG
	case "xiaomi":
		return pb.PhoneVendor_XIAOMI
	case "oppo":
		return pb.PhoneVendor_OPPO
	case "vivo":
		return pb.PhoneVendor_VIVO
	case "huawei":
		return pb.PhoneVendor_HUAWEI
	default:
		return pb.PhoneVendor_VENDOR_UNSPECIFIED
	}
}

// ProtocolFeatures 协议特定功能特性
type ProtocolFeatures struct {
	Protocol      ProtocolType
	Version       string
	Features      []string  // 支持的特性列表
	Limitations   []string  // 限制/不支持的特性
}

// GetFeatures 获取指定协议的功能特性
func GetFeatures(proto ProtocolType) *ProtocolFeatures {
	switch proto {
	case ProtocolCCC3:
		return &ProtocolFeatures{
			Protocol:    ProtocolCCC3,
			Version:     "3.0",
			Features:    []string{"BLE", "UWB", "PassiveEntry", "RemoteControl", "Sharing"},
			Limitations: []string{"NoNFC"},
		}
	case ProtocolICCOA40:
		return &ProtocolFeatures{
			Protocol:    ProtocolICCOA40,
			Version:     "4.0",
			Features:    []string{"BLE", "UWB", "NFC", "SE", "PassiveEntry", "RemoteControl", "Sharing", "ClimateControl", "FindCar"},
			Limitations: []string{},
		}
	case ProtocolICCOA30:
		return &ProtocolFeatures{
			Protocol:    ProtocolICCOA30,
			Version:     "3.0",
			Features:    []string{"BLE", "UWB", "SE", "PassiveEntry", "RemoteControl"},
			Limitations: []string{"NoNFC"},
		}
	case ProtocolICCE:
		return &ProtocolFeatures{
			Protocol:    ProtocolICCE,
			Version:     "2.0",
			Features:    []string{"BLE", "UWB", "NFC", "SE", "PassiveEntry", "RemoteControl", "Sharing", "Huawei Ecosystem"},
			Limitations: []string{"Vendor-specific (Huawei)"},
		}
	default:
		return nil
	}
}

// String 实现 fmt.Stringer
func (f *ProtocolFeatures) String() string {
	return fmt.Sprintf("%s v%s: %v (limitations: %v)", f.Protocol, f.Version, f.Features, f.Limitations)
}
