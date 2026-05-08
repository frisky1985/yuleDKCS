package unified

import (
	"fmt"
	"sort"
)

// Negotiator 协议协商器 - 根据设备能力和车辆支持情况选择最优协议
type Negotiator struct {
	supportedProtocols []ProtocolType
}

// NewNegotiator 创建协商器
func NewNegotiator(supportedProtocols []ProtocolType) *Negotiator {
	return &Negotiator{
		supportedProtocols: supportedProtocols,
	}
}

// NegotiateResult 协商结果
type NegotiateResult struct {
	Protocol   ProtocolType   // 协商确定的协议
	Version    string          // 协议版本
	MatchScore int             // 匹配度分数 (越高越好)
	Reason     string          // 选择原因
	MissingCaps []string       // 不满足的能力项
}

// Negotiate 根据设备能力和车辆信息协商最佳协议
// deviceCaps: 设备上报的能力集
// vehicleCaps: 车辆支持的能力集
func (n *Negotiator) Negotiate(deviceCaps, vehicleCaps *CapabilitySet) (*NegotiateResult, error) {
	if deviceCaps == nil || vehicleCaps == nil {
		return nil, fmt.Errorf("capability set is nil")
	}

	results := []*NegotiateResult{}
	
	for _, proto := range n.supportedProtocols {
		spec := GetSpec(proto)
		if spec == nil {
			continue
		}
		
		result := n.evaluate(proto, spec, deviceCaps, vehicleCaps)
		results = append(results, result)
	}
	
	// 按匹配度排序
	sort.Slice(results, func(i, j int) bool {
		return results[i].MatchScore > results[j].MatchScore
	})
	
	if len(results) == 0 {
		return nil, fmt.Errorf("no supported protocol available")
	}
	
	best := results[0]
	if best.MatchScore == 0 {
		return nil, fmt.Errorf("no compatible protocol: %v", best.MissingCaps)
	}
	
	return best, nil
}

// evaluate 评估单个协议的匹配度
func (n *Negotiator) evaluate(proto ProtocolType, spec *ProtocolSpec, deviceCaps, vehicleCaps *CapabilitySet) *NegotiateResult {
	score := 100
	missing := []string{}
	
	// 必须能力检查
	if spec.RequiredCaps == nil {
		return &NegotiateResult{
			Protocol: proto, MatchScore: 0, MissingCaps: []string{"spec has no required caps"},
		}
	}
	
	// 设备能力检查
	if spec.RequiredCaps.BLE && !deviceCaps.BLE {
		missing = append(missing, "BLE")
		score -= 40
	}
	if spec.RequiredCaps.UWB && !deviceCaps.UWB {
		missing = append(missing, "UWB")
		score -= 30
	}
	if spec.RequiredCaps.NFC && !deviceCaps.NFC {
		missing = append(missing, "NFC")
		score -= 20
	}
	if spec.RequiredCaps.SE && !deviceCaps.SE {
		missing = append(missing, "SE")
		score -= 30
	}
	if spec.RequiredCaps.FiRa && !deviceCaps.FiRa {
		missing = append(missing, "FiRa")
		score -= 20
	}
	
	// 车辆能力检查
	if spec.RequiredCaps.BLE && !vehicleCaps.BLE {
		missing = append(missing, "vehicle_BLE")
		score -= 40
	}
	if spec.RequiredCaps.UWB && !vehicleCaps.UWB {
		missing = append(missing, "vehicle_UWB")
		score -= 30
	}
	
	// 协议特定加成
	var reason string
	switch proto {
	case ProtocolICCOA40:
		// ICCOA 4.0 最先进
		reason = "ICCOA 4.0 is latest, supports NFC+UWB+BLE"
		score += 10
	case ProtocolCCC3:
		// CCC 3.0 标准化程度最高
		reason = "CCC 3.0 is cross-vendor standard"
		score += 5
	case ProtocolICCOA30:
		reason = "ICCOA 3.0 compatible"
		score += 0
	case ProtocolICCE:
		reason = "ICCE protocol (Huawei ecosystem)"
		score += 0
	}
	
	if score < 0 {
		score = 0
	}
	
	return &NegotiateResult{
		Protocol:   proto,
		Version:    spec.MinVersion,
		MatchScore: score,
		Reason:     reason,
		MissingCaps: missing,
	}
}

// RecommendOrder 返回推荐的协议优先级列表
// 用于设备端按顺序尝试
func (n *Negotiator) RecommendOrder(deviceCaps *CapabilitySet) []ProtocolType {
	// 优先推荐最新协议
	order := []ProtocolType{}
	
	// ICCOA 4.0 (最完善)
	if n.supports(ProtocolICCOA40) {
		order = append(order, ProtocolICCOA40)
	}
	// CCC 3.0 (标准化)
	if n.supports(ProtocolCCC3) {
		order = append(order, ProtocolCCC3)
	}
	// ICCOA 3.0 (兼容性)
	if n.supports(ProtocolICCOA30) {
		order = append(order, ProtocolICCOA30)
	}
	// ICCE (华为)
	if n.supports(ProtocolICCE) {
		order = append(order, ProtocolICCE)
	}
	
	return order
}

func (n *Negotiator) supports(p ProtocolType) bool {
	for _, sp := range n.supportedProtocols {
		if sp == p {
			return true
		}
	}
	return false
}

// AutoDetectProtocol 根据设备信息自动推断协议
func AutoDetectProtocol(vendor, os, appVersion string) ProtocolType {
	vendorLower := vendor
	// Android 厂商特定协议
	switch vendorLower {
	case "xiaomi":
		// 小米: ICCOA 4.0 > ICCOA 3.0
		return ProtocolICCOA40
	case "oppo":
		// OPPO: ICCOA
		return ProtocolICCOA40
	case "vivo":
		// vivo: ICCOA
		return ProtocolICCOA40
	case "huawei":
		// 华为: ICCE > ICCOA
		return ProtocolICCE
	case "samsung":
		// 三星: CCC 3.0 > ICCOA
		return ProtocolCCC3
	case "apple":
		// 苹果: CCC 3.0 独占
		return ProtocolCCC3
	default:
		// 未知厂商: 默认 ICCOA 4.0
		return ProtocolICCOA40
	}
}
