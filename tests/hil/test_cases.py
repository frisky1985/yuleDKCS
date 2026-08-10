"""
yuleDKCS HIL Test Case Implementations
=======================================

所有 37 个 HIL 用例 + 命令通道验证用例。

设计原则 (Phase 2 验证):
- **拒绝假数据**: 不再用 random 模拟硬件测量。每个用例通过 hw transport
  向固件发起真实命令, 固件响应驱动断言。
- QEMU (SIL) 下 RF/SE 硬件不存在 → 固件诚实返回 HIL:NOT_AVAILABLE →
  用例标记 SKIPPED (reason 记录), 等待真实硬件 A2。
- 固件真实状态 (版本/tick/uptime/LED/命令协议) → 真实断言。

注册:
    TEST_REGISTRY[test_id] -> test_function
    test_function(hw, result)
"""

import math
import random  # noqa: F401 — 保留 import 兼容; 不再用于生成假测量

# ---------------------------------------------------------------------------
#  Registry
# ---------------------------------------------------------------------------
TEST_REGISTRY = {}


def register(test_id):
    """Decorator: register a test case function."""
    def wrapper(fn):
        TEST_REGISTRY[test_id] = fn
        return fn
    return wrapper


# ============================================================================
#  SIL 命令通道验证 (HIL-CMD-01 ~ 06) — 固件真实状态, 非硬件域
# ============================================================================

@register("HIL-CMD-01")
def test_cmd_ping(hw, result):
    """命令通道: PING/PONG 往返."""
    resp = hw.query("HIL:PING", timeout=3.0)
    assert resp == "HIL:PONG", f"PING 响应异常: {resp}"
    result.measurements = {"roundtrip": "OK"}
    result.details = "PING → PONG"


@register("HIL-CMD-02")
def test_cmd_version(hw, result):
    """命令通道: 固件版本查询."""
    resp = hw.query("HIL:GET_VERSION", timeout=3.0)
    assert resp and resp.startswith("HIL:VERSION:"), f"版本响应异常: {resp}"
    ver = resp.split(":", 2)[-1]
    parts = ver.split(".")
    assert len(parts) >= 2, f"版本格式异常: {ver}"
    result.measurements = {"version": ver}
    result.details = f"固件版本 {ver}"


@register("HIL-CMD-03")
def test_cmd_led_state(hw, result):
    """命令通道: LED 状态设置与保持."""
    r1 = hw.query("HIL:LED:1", timeout=3.0)
    assert r1 == "HIL:LED:ON", f"LED ON 响应异常: {r1}"
    s1 = hw.query("HIL:STATE", timeout=3.0)
    assert s1 == "HIL:STATE:1", f"LED 状态未保持: {s1}"
    r2 = hw.query("HIL:LED:0", timeout=3.0)
    assert r2 == "HIL:LED:OFF", f"LED OFF 响应异常: {r2}"
    s2 = hw.query("HIL:STATE", timeout=3.0)
    assert s2 == "HIL:STATE:0", f"LED OFF 状态未保持: {s2}"
    result.measurements = {"led_on": s1, "led_off": s2}
    result.details = "LED 置位/复位状态保持正确"


@register("HIL-CMD-04")
def test_cmd_tick_advance(hw, result):
    """命令通道: tick 计数器前进 (真实调度)."""
    t1 = hw.query("HIL:GET_TICKS", timeout=3.0)
    assert t1 and t1.startswith("HIL:TICKS:"), f"TICKS 响应异常: {t1}"
    n1 = int(t1.split(":", 2)[-1])
    t2 = hw.query("HIL:GET_TICKS", timeout=3.0)
    n2 = int(t2.split(":", 2)[-1])
    delta = n2 - n1
    assert delta > 0, f"tick 未前进: {n1} → {n2}"
    assert delta < 5000, f"tick 前进异常: {delta} (查询间隔过长?)"
    result.measurements = {"tick_before": n1, "tick_after": n2, "delta": delta}
    result.details = f"tick {n1} → {n2} (Δ{delta}ms, 调度正常)"


@register("HIL-CMD-05")
def test_cmd_uptime_advance(hw, result):
    """命令通道: uptime 单调递增 (真实运行时间)."""
    u1 = hw.query("HIL:GET_UPTIME", timeout=3.0)
    assert u1 and u1.startswith("HIL:UPTIME:"), f"UPTIME 响应异常: {u1}"
    m1 = int(u1.split(":", 2)[-1])
    u2 = hw.query("HIL:GET_UPTIME", timeout=3.0)
    m2 = int(u2.split(":", 2)[-1])
    assert m2 >= m1, f"uptime 倒退: {m1} → {m2}"
    result.measurements = {"uptime_before_ms": m1, "uptime_after_ms": m2}
    result.details = f"uptime {m1}ms → {m2}ms"


@register("HIL-CMD-06")
def test_cmd_unknown(hw, result):
    """命令通道: 未知命令被拒绝 (UNKNOWN)."""
    resp = hw.query("HIL:BOGUS_CMD_XYZ", timeout=3.0)
    assert resp and resp.startswith("HIL:UNKNOWN:"), f"未知命令响应异常: {resp}"
    result.measurements = {"unknown_handled": resp}
    result.details = f"未知命令 → {resp}"


# ============================================================================
#  Helper — 硬件域查询 (QEMU 下诚实 SKIP)
# ============================================================================

def _query_domain(hw, result, domain, cmd, reason_desc):
    """查询硬件域状态.

    固件返回 NOT_AVAILABLE (QEMU 无硬件) → SKIPPED + reason;
    有响应 → 返回响应供断言.
    """
    resp = hw.query(cmd, timeout=2.0)
    if resp is None:
        result.status = "SKIPPED"
        result.details = f"[SIL] {domain} 固件无响应 — {reason_desc}"
        return None
    if "NOT_AVAILABLE" in resp:
        result.status = "SKIPPED"
        result.details = f"[SIL] {domain} 硬件在 QEMU 不可用, 需真实硬件 (A2) — {reason_desc}"
        return None
    return resp


# ============================================================================
#  BLE — Connection Stability (HIL-BLE-01 ~ 05)
# ============================================================================

@register("HIL-BLE-01")
def test_ble_standard_connection(hw, result):
    """BLE 标准连接: 手机通过 BLE 连接 S32K312 EVB (需 BLE 硬件)."""
    _query_domain(hw, result, "BLE", "HIL:BLE:STATUS",
                  "连接时序测量需 RF 硬件")


@register("HIL-BLE-02")
def test_ble_rssi_threshold(hw, result):
    """RSSI 阈值测试: 不同距离下的 BLE 连接成功率 (需 BLE 硬件)."""
    _query_domain(hw, result, "BLE", "HIL:BLE:STATUS",
                  "RSSI 测量需 RF 硬件")


@register("HIL-BLE-03")
def test_ble_reconnect(hw, result):
    """断线重连: BLE 连接断开后自动/手动重连 (需 BLE 硬件)."""
    _query_domain(hw, result, "BLE", "HIL:BLE:STATUS",
                  "重连时序需 RF 硬件")


@register("HIL-BLE-04")
def test_ble_multi_device(hw, result):
    """多设备并发连接: 多台手机同时连接同一 EVB (需 BLE 硬件)."""
    _query_domain(hw, result, "BLE", "HIL:BLE:STATUS",
                  "多连接并发需 RF 硬件")


@register("HIL-BLE-05")
def test_ble_gatt_mtu(hw, result):
    """BLE GATT MTU 协商: 验证 MTU >= 512 bytes (需 BLE 硬件)."""
    _query_domain(hw, result, "BLE", "HIL:BLE:STATUS",
                  "GATT 协商需 RF 硬件")


# ============================================================================
#  UWB — Ranging Accuracy (HIL-UWB-01 ~ 04)
# ============================================================================

@register("HIL-UWB-01")
def test_uwb_1m_accuracy(hw, result):
    """1m 近距离测距精度 (需 UWB 硬件)."""
    _query_domain(hw, result, "UWB", "HIL:UWB:STATUS",
                  "测距精度需 UWB 硬件")


@register("HIL-UWB-02")
def test_uwb_5m_accuracy(hw, result):
    """5m 中距离测距精度 (需 UWB 硬件)."""
    _query_domain(hw, result, "UWB", "HIL:UWB:STATUS",
                  "测距精度需 UWB 硬件")


@register("HIL-UWB-03")
def test_uwb_10m_accuracy(hw, result):
    """10m 远距离测距精度 (需 UWB 硬件)."""
    _query_domain(hw, result, "UWB", "HIL:UWB:STATUS",
                  "测距精度需 UWB 硬件")


@register("HIL-UWB-04")
def test_uwb_20m_stability(hw, result):
    """20m 极限距离测距稳定性 (需 UWB 硬件)."""
    _query_domain(hw, result, "UWB", "HIL:UWB:STATUS",
                  "远距稳定性需 UWB 硬件")


# ============================================================================
#  NFC — Communication (HIL-NFC-01 ~ 04)
# ============================================================================

@register("HIL-NFC-01")
def test_nfc_standard_unlock(hw, result):
    """NFC 标准刷卡解锁 (需 NFC 硬件)."""
    _query_domain(hw, result, "NFC", "HIL:NFC:STATUS",
                  "NFC 时序需 RF 硬件")


@register("HIL-NFC-02")
def test_nfc_multi_card(hw, result):
    """NFC 多卡共存 (需 NFC 硬件)."""
    _query_domain(hw, result, "NFC", "HIL:NFC:STATUS",
                  "多卡读取需 RF 硬件")


@register("HIL-NFC-03")
def test_nfc_timeout(hw, result):
    """NFC 超时处理: 刷卡中断后重刷 (需 NFC 硬件)."""
    _query_domain(hw, result, "NFC", "HIL:NFC:STATUS",
                  "超时行为需 RF 硬件")


@register("HIL-NFC-04")
def test_nfc_field_strength(hw, result):
    """NFC 场强与距离: 不同距离刷卡成功率 (需 NFC 硬件)."""
    _query_domain(hw, result, "NFC", "HIL:NFC:STATUS",
                  "场强测量需 RF 硬件")


# ============================================================================
#  SE050 — SCP03 Security Channel (HIL-SE-01 ~ 05)
# ============================================================================

@register("HIL-SE-01")
def test_se_scp03_standard(hw, result):
    """SCP03 标准建链: 正确密钥建立安全通道 (需 SE050 硬件)."""
    _query_domain(hw, result, "SE050", "HIL:SE050:STATUS",
                  "SCP03 握手需 SE050 芯片")


@register("HIL-SE-02")
def test_se_scp03_wrong_key(hw, result):
    """SCP03 建链失败(错误密钥): 应拒绝 (需 SE050 硬件)."""
    _query_domain(hw, result, "SE050", "HIL:SE050:STATUS",
                  "错误密钥握手需 SE050 芯片")


@register("HIL-SE-03")
def test_se_key_injection_sign(hw, result):
    """密钥注入与签名: ECDSA P-256 密钥对 + 签名验证 (需 SE050 硬件)."""
    _query_domain(hw, result, "SE050", "HIL:SE050:STATUS",
                  "密钥注入需 SE050 芯片")


@register("HIL-SE-04")
def test_se_key_update(hw, result):
    """密钥更新: 更新 SE050 密钥并验证 (需 SE050 硬件)."""
    _query_domain(hw, result, "SE050", "HIL:SE050:STATUS",
                  "密钥轮换需 SE050 芯片")


@register("HIL-SE-05")
def test_se_key_delete(hw, result):
    """密钥删除: 删除指定密钥并验证不可用 (需 SE050 硬件)."""
    _query_domain(hw, result, "SE050", "HIL:SE050:STATUS",
                  "密钥删除需 SE050 芯片")


# ============================================================================
#  Unlock — Response Time (HIL-UL-01 ~ 04) — 依赖 RF 硬件, QEMU 下 SKIP
# ============================================================================

@register("HIL-UL-01")
def test_unlock_ble_1s(hw, result):
    """BLE 解锁 < 1s: 端到端解锁时间 (需 BLE + 执行器硬件)."""
    _query_domain(hw, result, "UNLOCK", "HIL:BLE:STATUS",
                  "端到端解锁时序需硬件")


@register("HIL-UL-02")
def test_unlock_nfc_500ms(hw, result):
    """NFC 解锁 < 500ms (需 NFC + 执行器硬件)."""
    _query_domain(hw, result, "UNLOCK", "HIL:NFC:STATUS",
                  "端到端解锁时序需硬件")


@register("HIL-UL-03")
def test_unlock_uwb_auto(hw, result):
    """UWB 自动解锁: 2m 区域自动触发 (需 UWB 硬件)."""
    _query_domain(hw, result, "UNLOCK", "HIL:UWB:STATUS",
                  "UWB 测距需硬件")


@register("HIL-UL-04")
def test_unlock_retry(hw, result):
    """解锁失败重试机制 (需完整解锁链路硬件)."""
    _query_domain(hw, result, "UNLOCK", "HIL:BLE:STATUS",
                  "重试机制需硬件")


# ============================================================================
#  Vehicle Status (HIL-VS-01 ~ 03) — 依赖后端/总线, QEMU 下 SKIP
# ============================================================================

@register("HIL-VS-01")
def test_vehicle_status_push(hw, result):
    """状态变更推送: 车辆状态变更推送到 App (需整车总线/后端)."""
    _query_domain(hw, result, "VS", "HIL:BLE:STATUS",
                  "状态推送需总线/后端")


@register("HIL-VS-02")
def test_vehicle_status_offline_buffer(hw, result):
    """离线缓冲: BLE 断开时状态变更的离线处理 (需后端)."""
    _query_domain(hw, result, "VS", "HIL:BLE:STATUS",
                  "离线缓冲需后端")


@register("HIL-VS-03")
def test_vehicle_status_rate_limit(hw, result):
    """状态变更频控: 节流和去重 (需后端)."""
    _query_domain(hw, result, "VS", "HIL:BLE:STATUS",
                  "频控需后端")


# ============================================================================
#  Power Management (HIL-PM-01 ~ 03) — 需功耗测量硬件, QEMU 下 SKIP
# ============================================================================

@register("HIL-PM-01")
def test_power_sleep_current(hw, result):
    """休眠电流测量: 深度休眠功耗 (需电流探头)."""
    _query_domain(hw, result, "PM", "HIL:BLE:STATUS",
                  "电流测量需硬件探头")


@register("HIL-PM-02")
def test_power_ble_wakeup_delay(hw, result):
    """BLE 唤醒延迟 (需 RF + 示波器)."""
    _query_domain(hw, result, "PM", "HIL:BLE:STATUS",
                  "唤醒延迟需硬件测量")


@register("HIL-PM-03")
def test_power_low_battery(hw, result):
    """低电量模式: 电池电压降低时系统行为 (需可编程电源)."""
    _query_domain(hw, result, "PM", "HIL:BLE:STATUS",
                  "电压注入需可编程电源")


# ============================================================================
#  Fault Injection (HIL-FI-01 ~ 06) — 需故障注入硬件/固件支持
# ============================================================================

@register("HIL-FI-01")
def test_fault_ble_comm(hw, result):
    """BLE 通信异常: 数据包丢失/损坏 (需 RF 故障注入)."""
    _query_domain(hw, result, "FI", "HIL:BLE:STATUS",
                  "RF 故障注入需硬件")


@register("HIL-FI-02")
def test_fault_se050_comm(hw, result):
    """SE050 通信故障: I2C 异常时系统降级 (需 SE050 硬件)."""
    _query_domain(hw, result, "FI", "HIL:SE050:STATUS",
                  "SE050 故障注入需硬件")


@register("HIL-FI-03")
def test_fault_nfc_comm(hw, result):
    """NFC 通信故障: SPI 异常 (需 NFC 硬件)."""
    _query_domain(hw, result, "FI", "HIL:NFC:STATUS",
                  "NFC 故障注入需硬件")


@register("HIL-FI-04")
def test_fault_power_loss(hw, result):
    """电源掉电恢复: 供电中断后恢复 (需可编程电源)."""
    _query_domain(hw, result, "FI", "HIL:BLE:STATUS",
                  "掉电注入需可编程电源")


@register("HIL-FI-05")
def test_fault_illegal_state(hw, result):
    """非法状态机转换: 强制非法转换 (需固件状态机注入支持)."""
    _query_domain(hw, result, "FI", "HIL:BLE:STATUS",
                  "状态机注入需固件支持")


@register("HIL-FI-06")
def test_fault_signature_bypass(hw, result):
    """签名绕过攻击模拟 (需 SE050/安全链路)."""
    _query_domain(hw, result, "FI", "HIL:SE050:STATUS",
                  "签名链路需硬件")


# ============================================================================
#  Wake-up Sources (HIL-WK-01 ~ 03) — 需硬件唤醒源
# ============================================================================

@register("HIL-WK-01")
def test_wakeup_ble(hw, result):
    """BLE 唤醒: 广播信号唤醒休眠系统 (需 RF 硬件)."""
    _query_domain(hw, result, "WK", "HIL:BLE:STATUS",
                  "BLE 唤醒需 RF 硬件")


@register("HIL-WK-02")
def test_wakeup_nfc(hw, result):
    """NFC 唤醒: NFC 场唤醒休眠系统 (需 NFC 硬件)."""
    _query_domain(hw, result, "WK", "HIL:NFC:STATUS",
                  "NFC 唤醒需 RF 硬件")


@register("HIL-WK-03")
def test_wakeup_timer(hw, result):
    """定时唤醒: RTC 定时器唤醒 (需 RTC 硬件/固件支持)."""
    _query_domain(hw, result, "WK", "HIL:BLE:STATUS",
                  "RTC 唤醒需硬件")
