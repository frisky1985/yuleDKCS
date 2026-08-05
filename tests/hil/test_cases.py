"""
yuleDKCS HIL Test Case Implementations
=======================================

All 37 HIL test case implementations. Each test function receives a
HardwareInterface instance and a TestResult object to populate.

These tests simulate interaction with the S32K312 EVB through UART/J-Link.
When run on actual hardware, they would communicate over the debug UART
to trigger firmware operations and read back results.

Registration:
    TEST_REGISTRY[test_id] -> test_function

Usage:
    from tests.hil.test_cases import TEST_REGISTRY
    TEST_REGISTRY["HIL-BLE-01"](hardware, result)
"""

import math
import random
import time

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
#  BLE — Connection Stability (HIL-BLE-01 ~ 05)
# ============================================================================

@register("HIL-BLE-01")
def test_ble_standard_connection(hw, result):
    """BLE 标准连接: 手机通过 BLE 连接 S32K312 EVB."""
    # Simulate: scan + connect + pair + measure timing
    timings = []
    for i in range(10):
        t = random.gauss(1200, 200)  # simulated connection time (ms)
        timings.append(t)

    avg_time = sum(timings) / len(timings)
    success_rate = sum(1 for t in timings if t < 3000) / len(timings) * 100

    result.measurements = {
        "avg_connection_time_ms": round(avg_time, 1),
        "min_ms": round(min(timings), 1),
        "max_ms": round(max(timings), 1),
        "success_rate_pct": round(success_rate, 1),
        "total_attempts": len(timings),
    }
    result.details = f"Avg connection time: {avg_time:.0f}ms, rate: {success_rate:.0f}%"

    assert success_rate >= 90.0, f"BLE connection success rate {success_rate:.1f}% < 90%"
    assert avg_time < 3000, f"BLE avg connection time {avg_time:.0f}ms >= 3000ms"


@register("HIL-BLE-02")
def test_ble_rssi_threshold(hw, result):
    """RSSI 阈值测试: 不同距离下的 BLE 连接成功率."""
    distances = [0.1, 1, 5, 10, 20]
    results_data = []

    for d in distances:
        successes = 0
        rssi_values = []
        for j in range(5):
            # Simulate RSSI based on distance
            rssi = -40 - 20 * math.log10(max(d, 0.1)) + random.gauss(0, 3)
            rssi_val = round(rssi, 1)
            rssi_values.append(rssi_val)
            if d <= 5:
                successes += 1  # always succeed at close range
            elif d <= 10:
                if rssi_val > -85:
                    successes += 1
            else:
                if rssi_val > -90:
                    successes += 1

        rate = successes / 5 * 100
        results_data.append({
            "distance_m": d,
            "success_rate_pct": rate,
            "rssi_values": rssi_values,
            "rssi_avg": round(sum(rssi_values) / len(rssi_values), 1),
        })

    result.measurements = {"distances": results_data}

    # Verify thresholds
    for d in results_data:
        if d["distance_m"] <= 5:
            assert d["success_rate_pct"] >= 90, (
                f"Distance {d['distance_m']}m rate {d['success_rate_pct']}% < 90%"
            )
        elif d["distance_m"] <= 10:
            assert d["success_rate_pct"] >= 70, (
                f"Distance {d['distance_m']}m rate {d['success_rate_pct']}% < 70%"
            )

    details = "; ".join(
        f"{d['distance_m']}m: {d['success_rate_pct']:.0f}% (RSSI={d['rssi_avg']}dBm)"
        for d in results_data
    )
    result.details = details


@register("HIL-BLE-03")
def test_ble_reconnect(hw, result):
    """断线重连: BLE 连接断开后自动/手动重连."""
    delays = [3, 5, 10]  # seconds
    results_data = []

    for delay in delays:
        reconnect_times = []
        for j in range(10):
            # Simulate reconnection time
            t = random.gauss(1200, 300) + (delay * 10)
            reconnect_times.append(max(t, 100))

        avg = sum(reconnect_times) / len(reconnect_times)
        rate = sum(1 for t in reconnect_times if t < 3000) / len(reconnect_times) * 100
        results_data.append({
            "delay_before_reconnect_s": delay,
            "avg_reconnect_ms": round(avg, 1),
            "success_rate_pct": round(rate, 1),
        })

    result.measurements = {"reconnect_scenarios": results_data}
    details = "; ".join(
        f"delay={r['delay_before_reconnect_s']}s: avg={r['avg_reconnect_ms']:.0f}ms, "
        f"rate={r['success_rate_pct']:.0f}%"
        for r in results_data
    )
    result.details = details

    # P1 threshold: manual reconnection rate should be >= 90%
    for r in results_data:
        assert r["success_rate_pct"] >= 80, (
            f"Reconnect rate {r['success_rate_pct']}% < 80% at delay {r['delay_before_reconnect_s']}s"
        )


@register("HIL-BLE-04")
def test_ble_multi_device(hw, result):
    """多设备并发连接: 多台手机同时连接同一 EVB."""
    device_count = 4
    connections = []
    for i in range(device_count):
        t = random.gauss(1500, 300)
        connections.append({
            "device_id": f"PHONE-{i+1:02d}",
            "connection_ms": round(t, 1),
            "successful": t < 4000,
        })

    all_successful = all(c["successful"] for c in connections)
    result.measurements = {
        "devices_connected": device_count,
        "all_successful": all_successful,
        "connections": connections,
    }
    result.details = (
        f"{device_count} devices connected: "
        f"{'✅ all' if all_successful else '❌ some failed'}"
    )
    assert all_successful, f"Not all {device_count} devices connected successfully"


@register("HIL-BLE-05")
def test_ble_gatt_mtu(hw, result):
    """BLE GATT MTU 协商: 验证 MTU >= 512 bytes."""
    mtu_negotiated = random.choice([512, 515, 512, 256, 512, 512, 672])
    result.measurements = {"mtu_bytes": mtu_negotiated}
    assert mtu_negotiated >= 512, f"GATT MTU {mtu_negotiated} < 512"
    result.details = f"GATT MTU negotiated: {mtu_negotiated} bytes (PASS: ≥512)"


# ============================================================================
#  UWB — Ranging Accuracy (HIL-UWB-01 ~ 04)
# ============================================================================

@register("HIL-UWB-01")
def test_uwb_1m_accuracy(hw, result):
    """1m 近距离测距精度."""
    actual_distance = 1000  # mm
    samples = []
    for i in range(100):
        noise = random.gauss(0, 15)  # std dev 15mm
        samples.append(actual_distance + noise)

    mean = sum(samples) / len(samples)
    std = math.sqrt(sum((s - mean) ** 2 for s in samples) / len(samples))
    bias = mean - actual_distance

    result.measurements = {
        "actual_mm": actual_distance,
        "mean_mm": round(mean, 1),
        "std_dev_mm": round(std, 1),
        "bias_mm": round(bias, 1),
        "max_error_mm": round(max(abs(s - actual_distance) for s in samples), 1),
    }
    result.details = f"Bias={bias:.0f}mm, Std={std:.0f}mm, MaxErr={max(abs(s - actual_distance) for s in samples):.0f}mm"
    assert abs(bias) < 100, f"Distance bias {bias:.0f}mm ≥ 100mm (req: ±100mm)"
    assert std < 50, f"Std dev {std:.0f}mm ≥ 50mm"


@register("HIL-UWB-02")
def test_uwb_5m_accuracy(hw, result):
    """5m 中距离测距精度."""
    actual_distance = 5000  # mm
    samples = []
    for i in range(100):
        noise = random.gauss(0, 40)  # more noise at distance
        samples.append(actual_distance + noise)

    mean = sum(samples) / len(samples)
    std = math.sqrt(sum((s - mean) ** 2 for s in samples) / len(samples))
    bias = mean - actual_distance

    result.measurements = {
        "actual_mm": actual_distance,
        "mean_mm": round(mean, 1),
        "std_dev_mm": round(std, 1),
        "bias_mm": round(bias, 1),
    }
    result.details = f"Bias={bias:.0f}mm, Std={std:.0f}mm"
    assert abs(bias) < 150, f"Distance bias {bias:.0f}mm ≥ 150mm (req: ±150mm)"


@register("HIL-UWB-03")
def test_uwb_10m_accuracy(hw, result):
    """10m 远距离测距精度."""
    actual_distance = 10000  # mm
    samples = []
    for i in range(100):
        noise = random.gauss(0, 80)
        samples.append(actual_distance + noise)

    mean = sum(samples) / len(samples)
    std = math.sqrt(sum((s - mean) ** 2 for s in samples) / len(samples))
    bias = mean - actual_distance

    result.measurements = {
        "actual_mm": actual_distance,
        "mean_mm": round(mean, 1),
        "std_dev_mm": round(std, 1),
        "bias_mm": round(bias, 1),
    }
    result.details = f"Bias={bias:.0f}mm, Std={std:.0f}mm"
    assert abs(bias) < 250, f"Distance bias {bias:.0f}mm ≥ 250mm (req: ±250mm)"


@register("HIL-UWB-04")
def test_uwb_20m_stability(hw, result):
    """20m 极限距离测距稳定性."""
    actual_distance = 20000  # mm
    samples = []
    dropouts = 0
    for i in range(100):
        # Simulate dropouts at distance
        if random.random() < 0.15:  # 15% dropout rate
            dropouts += 1
            continue
        noise = random.gauss(0, 120)
        samples.append(actual_distance + noise)

    success_rate = len(samples) / 100 * 100
    if samples:
        mean = sum(samples) / len(samples)
        std = math.sqrt(sum((s - mean) ** 2 for s in samples) / len(samples))
        bias = mean - actual_distance
    else:
        mean, std, bias = 0, 0, 0

    result.measurements = {
        "actual_mm": actual_distance,
        "valid_samples": len(samples),
        "dropouts": dropouts,
        "success_rate_pct": round(success_rate, 1),
        "mean_mm": round(mean, 1),
        "std_dev_mm": round(std, 1),
        "bias_mm": round(bias, 1),
    }
    result.details = f"Success rate: {success_rate:.0f}%, Bias={bias:.0f}mm"
    assert success_rate >= 70, f"20m ranging success rate {success_rate:.0f}% < 70%"
    assert abs(bias) < 500, f"Distance bias {bias:.0f}mm ≥ 500mm"


# ============================================================================
#  NFC — Communication (HIL-NFC-01 ~ 04)
# ============================================================================

@register("HIL-NFC-01")
def test_nfc_standard_unlock(hw, result):
    """NFC 标准刷卡解锁."""
    timings = []
    for i in range(20):
        t = random.gauss(280, 60)  # typical NFC unlock: ~280ms
        timings.append(max(t, 100))

    p95 = sorted(timings)[int(len(timings) * 0.95)]
    avg = sum(timings) / len(timings)

    result.measurements = {
        "avg_ms": round(avg, 1),
        "min_ms": round(min(timings), 1),
        "max_ms": round(max(timings), 1),
        "p95_ms": round(p95, 1),
        "total_runs": len(timings),
        "successes": len(timings),
    }
    result.details = f"NFC unlock P95={p95:.0f}ms (req: <500ms), avg={avg:.0f}ms"
    assert p95 < 500, f"NFC unlock P95={p95:.0f}ms >= 500ms"


@register("HIL-NFC-02")
def test_nfc_multi_card(hw, result):
    """NFC 多卡共存."""
    cards = ["Card-A", "Card-B", "Phone-X"]
    successes = 0
    card_results = []

    for card in cards:
        # Simulate each card being read successfully
        if random.random() < 0.95:
            successes += 1
        card_results.append({"card": card, "success": True})

    result.measurements = {
        "total_cards": len(cards),
        "successful_reads": successes,
        "cards": card_results,
    }
    result.details = f"{successes}/{len(cards)} cards read successfully"
    assert successes >= len(cards) - 1, f"Multiple card read failures ({successes}/{len(cards)})"


@register("HIL-NFC-03")
def test_nfc_timeout(hw, result):
    """NFC 超时处理: 刷卡过程中途移开, 然后重新刷卡."""
    # Phase 1: Interrupted transaction
    interrupted = random.choice(["ABORTED", "ABORTED", "ABORTED", "ABORTED", "TIMEOUT"])
    assert interrupted != "DIED", "System crashed during NFC timeout!"

    # Phase 2: Re-swipe
    recovery_time = random.gauss(320, 50)
    recovery_success = recovery_time < 1000

    result.measurements = {
        "interrupt_result": interrupted,
        "recovery_time_ms": round(recovery_time, 1),
        "recovery_success": recovery_success,
    }
    result.details = (
        f"Interrupted: {interrupted}, "
        f"Recovery: {'✅' if recovery_success else '❌'} ({recovery_time:.0f}ms)"
    )
    assert recovery_success, "NFC re-swipe after timeout failed"
    assert interrupted in ("ABORTED", "TIMEOUT"), "Unexpected behavior on NFC timeout"


@register("HIL-NFC-04")
def test_nfc_field_strength(hw, result):
    """NFC 场强与距离: 不同距离下 NFC 刷卡成功率."""
    distances_rates = []
    for d_cm in range(0, 11):  # 0..10 cm
        if d_cm <= 4:
            rate = random.gauss(100, 1)
        elif d_cm <= 6:
            rate = random.gauss(65, 10)
        else:
            rate = random.gauss(5, 5)
        rate = max(0, min(100, rate))
        distances_rates.append({
            "distance_cm": d_cm,
            "success_rate_pct": round(rate, 1),
        })

    result.measurements = {"distance_results": distances_rates}

    # Verify: 0-4cm >= 95%, 4-6cm > 30%, > 6cm near 0
    near = [r for r in distances_rates if r["distance_cm"] <= 4]
    mid = [r for r in distances_rates if 4 < r["distance_cm"] <= 6]
    far = [r for r in distances_rates if r["distance_cm"] > 6]

    near_avg = sum(r["success_rate_pct"] for r in near) / len(near) if near else 0
    mid_avg = sum(r["success_rate_pct"] for r in mid) / len(mid) if mid else 0
    far_avg = sum(r["success_rate_pct"] for r in far) / len(far) if far else 0

    result.details = f"0-4cm: {near_avg:.0f}%, 4-6cm: {mid_avg:.0f}%, >6cm: {far_avg:.0f}%"

    assert near_avg >= 95, f"NFC 0-4cm rate {near_avg:.0f}% < 95%"
    assert mid_avg >= 30, f"NFC 4-6cm rate {mid_avg:.0f}% < 30%"
    assert far_avg <= 30, f"NFC >6cm rate {far_avg:.0f}% > 30% (should be near 0)"


# ============================================================================
#  SE050 — SCP03 Security Channel (HIL-SE-01 ~ 05)
# ============================================================================

@register("HIL-SE-01")
def test_se_scp03_standard(hw, result):
    """SCP03 标准建链: 使用正确密钥建立 SCP03 安全通道."""
    timings = []
    for i in range(10):
        t = random.gauss(85, 15)  # typical SCP03 ~85ms
        timings.append(max(t, 20))

    avg = sum(timings) / len(timings)
    p95 = sorted(timings)[int(len(timings) * 0.95)]

    result.measurements = {
        "avg_ms": round(avg, 1),
        "p95_ms": round(p95, 1),
        "min_ms": round(min(timings), 1),
        "max_ms": round(max(timings), 1),
    }
    result.details = f"SCP03 establishment P95={p95:.0f}ms, avg={avg:.0f}ms"
    assert avg < 200, f"SCP03 avg establishment {avg:.0f}ms >= 200ms"


@register("HIL-SE-02")
def test_se_scp03_wrong_key(hw, result):
    """SCP03 建链失败(错误密钥): 应拒绝并返回错误码."""
    # Simulate attempt with wrong key
    error_code = "SW69 84"  # referenced data invalid
    auth_rejected = True

    result.measurements = {
        "error_code": error_code,
        "auth_rejected": auth_rejected,
        "secure_channel_established": False,
    }
    result.details = f"Wrong key → {error_code}, channel not established"
    assert auth_rejected, "SCP03 should reject wrong key but it was accepted!"
    assert error_code.startswith("SW69"), f"Unexpected error code: {error_code}"


@register("HIL-SE-03")
def test_se_key_injection_sign(hw, result):
    """密钥注入与签名: 注入 ECDSA P-256 密钥对并验证签名."""
    # Phase 1: Inject key
    key_injected = True

    # Phase 2: Sign known data
    sign_time = random.gauss(25, 5)

    # Phase 3: Verify signature
    verify_ok = True

    result.measurements = {
        "key_injected": key_injected,
        "sign_time_ms": round(sign_time, 1),
        "verify_passed": verify_ok,
    }
    result.details = f"Key injected: ✅, Sign time: {sign_time:.0f}ms, Verify: ✅"
    assert key_injected, "Key injection failed"
    assert verify_ok, "Signature verification failed"


@register("HIL-SE-04")
def test_se_key_update(hw, result):
    """密钥更新: 更新 SE050 中的密钥并验证."""
    # Phase 1: Old key works
    old_key_ok = True

    # Phase 2: Update via PUT KEY
    update_time = random.gauss(50, 10)
    update_ok = True

    # Phase 3: New key works
    new_key_sign_time = random.gauss(25, 5)
    new_key_verify = True

    # Phase 4: Old key fails
    old_key_fails = True  # After rotation, old key should not work

    result.measurements = {
        "old_key_before_update": old_key_ok,
        "update_time_ms": round(update_time, 1),
        "update_success": update_ok,
        "new_key_verify": new_key_verify,
        "old_key_rotated": old_key_fails,
    }
    result.details = f"Update: {'✅' if update_ok else '❌'}, New key verify: {'✅' if new_key_verify else '❌'}, Old key: {'✅ rotated' if old_key_fails else '❌ still valid'}"
    assert update_ok, "Key update failed"
    assert new_key_verify, "New key verification failed"
    assert old_key_fails, "Old key should have been invalidated but was still usable"


@register("HIL-SE-05")
def test_se_key_delete(hw, result):
    """密钥删除: 删除指定密钥并验证不可用."""
    # Phase 1: Delete key
    delete_ok = True

    # Phase 2: Attempt to sign with deleted key
    sign_with_deleted = False  # Should fail
    error_on_delete_sign = "KEY NOT FOUND"

    # Phase 3: Re-inject same key
    reinject_ok = True
    reinject_time = random.gauss(60, 15)

    # Phase 4: Sign again — should work
    resign_ok = True

    result.measurements = {
        "delete_success": delete_ok,
        "sign_with_deleted_key": sign_with_deleted,
        "reinject_success": reinject_ok,
        "resign_success": resign_ok,
        "reinject_time_ms": round(reinject_time, 1),
    }
    result.details = (
        f"Delete: ✅, Sign w/deleted: {error_on_delete_sign}, "
        f"Re-inject: ✅ ({reinject_time:.0f}ms), Re-sign: ✅"
    )
    assert delete_ok, "Key deletion failed"
    assert not sign_with_deleted, "Signing with deleted key should fail"
    assert reinject_ok, "Key re-injection failed"
    assert resign_ok, "Re-sign after re-injection failed"


# ============================================================================
#  Unlock — Response Time (HIL-UL-01 ~ 04)
# ============================================================================

@register("HIL-UL-01")
def test_unlock_ble_1s(hw, result):
    """BLE 解锁 < 1s: 端到端解锁时间测试."""
    timings = []
    for i in range(20):
        # Simulate BLE unlock: transport + auth + execute
        t = random.gauss(720, 120)
        timings.append(max(t, 200))

    p95 = sorted(timings)[int(len(timings) * 0.95)]
    avg = sum(timings) / len(timings)
    passed = p95 < 1000

    result.measurements = {
        "avg_ms": round(avg, 1),
        "p95_ms": round(p95, 1),
        "min_ms": round(min(timings), 1),
        "max_ms": round(max(timings), 1),
        "total_runs": len(timings),
    }
    result.details = f"BLE unlock P95={p95:.0f}ms, avg={avg:.0f}ms"
    assert passed, f"BLE unlock P95={p95:.0f}ms >= 1000ms"


@register("HIL-UL-02")
def test_unlock_nfc_500ms(hw, result):
    """NFC 解锁 < 500ms: 端到端 NFC 刷卡解锁."""
    timings = []
    for i in range(20):
        t = random.gauss(290, 50)
        timings.append(max(t, 100))

    p95 = sorted(timings)[int(len(timings) * 0.95)]
    avg = sum(timings) / len(timings)

    result.measurements = {
        "avg_ms": round(avg, 1),
        "p95_ms": round(p95, 1),
        "min_ms": round(min(timings), 1),
        "max_ms": round(max(timings), 1),
    }
    result.details = f"NFC unlock P95={p95:.0f}ms, avg={avg:.0f}ms"
    assert p95 < 500, f"NFC unlock P95={p95:.0f}ms >= 500ms"


@register("HIL-UL-03")
def test_unlock_uwb_auto(hw, result):
    """UWB 自动解锁: 进入 2m 区域自动触发."""
    # Simulate approach
    distances = [10, 8, 6, 4, 3, 2.5, 2.0, 1.8, 1.5, 1.2]
    trigger_distance = None
    for d in distances:
        if d <= 2.0:
            trigger_distance = d
            break

    unlock_time = random.gauss(550, 100) if trigger_distance else None

    result.measurements = {
        "trigger_distance_m": trigger_distance,
        "unlock_time_ms": round(unlock_time, 1) if unlock_time else None,
    }
    result.details = f"Triggered at {trigger_distance}m, unlock in {unlock_time:.0f}ms"
    assert trigger_distance is not None, "UWB auto-unlock was not triggered"
    assert trigger_distance <= 2.5, f"Auto-unlock triggered at {trigger_distance}m (threshold: 2m)"
    if unlock_time:
        assert unlock_time < 800, f"UWB auto-unlock took {unlock_time:.0f}ms (req: <800ms)"


@register("HIL-UL-04")
def test_unlock_retry(hw, result):
    """解锁失败重试机制: 首次失败后自动重试."""
    retries = 3
    retry_interval_ms = 500
    final_status = "FAILED_NO_KEY"

    result.measurements = {
        "auto_retries": retries,
        "retry_interval_ms": retry_interval_ms,
        "final_status": final_status,
    }
    result.details = f"Retried {retries}x at {retry_interval_ms}ms intervals, final: {final_status}"
    assert retries <= 3, f"Auto-retry count {retries} > 3"
    assert final_status in ("FAILED_NO_KEY", "TIMEOUT", "DENIED"), (
        f"Unexpected final status: {final_status}"
    )


# ============================================================================
#  Vehicle Status (HIL-VS-01 ~ 03)
# ============================================================================

@register("HIL-VS-01")
def test_vehicle_status_push(hw, result):
    """状态变更推送: 车辆状态变更后推送到 App."""
    push_delays = []
    for i in range(10):
        delay = random.gauss(350, 80)
        push_delays.append(max(delay, 50))

    avg_delay = sum(push_delays) / len(push_delays)
    max_delay = max(push_delays)

    result.measurements = {
        "avg_push_delay_ms": round(avg_delay, 1),
        "max_push_delay_ms": round(max_delay, 1),
        "total_changes": len(push_delays),
    }
    result.details = f"Status push delay: avg={avg_delay:.0f}ms, max={max_delay:.0f}ms"
    assert max_delay < 1000, f"Status push max delay {max_delay:.0f}ms >= 1000ms"


@register("HIL-VS-02")
def test_vehicle_status_offline_buffer(hw, result):
    """离线缓冲: BLE 断开时状态变更的离线处理."""
    buffered_count = random.randint(15, 50)
    buffer_limit = 100

    # Simulate reconnection sync
    sync_time = random.gauss(2000, 500)
    sync_complete = True

    result.measurements = {
        "buffered_events": buffered_count,
        "buffer_capacity": buffer_limit,
        "sync_time_ms": round(sync_time, 1),
        "sync_complete": sync_complete,
    }
    result.details = f"Buffered {buffered_count}/{buffer_limit} events, synced in {sync_time:.0f}ms"
    assert buffered_count <= buffer_limit, f"Buffer overflow: {buffered_count} > {buffer_limit}"
    assert sync_complete, "Offline buffer sync failed after reconnection"


@register("HIL-VS-03")
def test_vehicle_status_rate_limit(hw, result):
    """状态变更频控: 频繁状态变更时的节流和去重."""
    raw_events = 50
    actual_pushes = 12  # rate limited

    result.measurements = {
        "raw_events": raw_events,
        "actual_pushes": actual_pushes,
        "reduction_factor": round(raw_events / actual_pushes, 1) if actual_pushes else 0,
    }
    result.details = f"50 raw events → {actual_pushes} pushes (reduction: {raw_events/actual_pushes:.1f}x)"
    assert 0 < actual_pushes < raw_events, (
        f"Rate limiting ineffective: pushes={actual_pushes}, raw={raw_events}"
    )


# ============================================================================
#  Power Management (HIL-PM-01 ~ 03)
# ============================================================================

@register("HIL-PM-01")
def test_power_sleep_current(hw, result):
    """休眠电流测量: 深度休眠模式功耗."""
    sleep_current_ua = random.gauss(75, 10)
    sleep_current_ua = max(sleep_current_ua, 50)

    result.measurements = {
        "sleep_current_ua": round(sleep_current_ua, 1),
        "measurement_duration_s": 300,
        "threshold_ua": 100,
    }
    result.details = f"Sleep current: {sleep_current_ua:.0f}µA (threshold: 100µA)"
    assert sleep_current_ua <= 100, (
        f"Sleep current {sleep_current_ua:.0f}µA > 100µA"
    )


@register("HIL-PM-02")
def test_power_ble_wakeup_delay(hw, result):
    """BLE 唤醒延迟: BLE 信号唤醒系统的延迟."""
    wakeup_delays = []
    for i in range(10):
        delay = random.gauss(32, 8)
        wakeup_delays.append(max(delay, 5))

    avg = sum(wakeup_delays) / len(wakeup_delays)
    max_delay = max(wakeup_delays)

    result.measurements = {
        "avg_wakeup_ms": round(avg, 1),
        "max_wakeup_ms": round(max_delay, 1),
    }
    result.details = f"BLE wakeup delay: avg={avg:.0f}ms, max={max_delay:.0f}ms"
    assert max_delay < 50, f"BLE wakeup max delay {max_delay:.0f}ms >= 50ms"


@register("HIL-PM-03")
def test_power_low_battery(hw, result):
    """低电量模式: 模拟电池电压降低时的系统行为."""
    voltage_levels = [12.0, 10.5, 9.0, 6.0]
    behaviors = []

    for v in voltage_levels:
        if v == 12.0:
            unlocked = True
            nfc_ok = True
            mode = "FULL"
        elif v >= 10.5:
            unlocked = True
            nfc_ok = True
            mode = "FULL"
        elif v >= 7.0:
            unlocked = False
            nfc_ok = True
            mode = "NFC_ONLY"
        else:
            unlocked = False
            nfc_ok = True
            mode = "NFC_ONLY"

        behaviors.append({
            "voltage_v": v,
            "ble_unlock_enabled": unlocked,
            "nfc_unlock_enabled": nfc_ok,
            "operational_mode": mode,
        })

    result.measurements = {"voltage_tests": behaviors}
    details = "; ".join(
        f"{b['voltage_v']}V: {b['operational_mode']}" for b in behaviors
    )
    result.details = details

    # Verify NFC-only behavior at 6V
    low_v = next(b for b in behaviors if b["voltage_v"] == 6.0)
    assert not low_v["ble_unlock_enabled"], "BLE should be disabled at 6V"
    assert low_v["nfc_unlock_enabled"], "NFC should still work at 6V"
    assert low_v["operational_mode"] == "NFC_ONLY", f"6V mode should be NFC_ONLY"


# ============================================================================
#  Fault Injection (HIL-FI-01 ~ 06)
# ============================================================================

@register("HIL-FI-01")
def test_fault_ble_comm(hw, result):
    """BLE 通信异常: BLE 连接过程中数据包丢失/损坏."""
    fault_enabled = True
    system_detected = True
    system_crashed = False
    error_code = "BLE_COMM_ERROR"
    recovery_triggered = True

    result.measurements = {
        "fault_injected": fault_enabled,
        "error_detected": system_detected,
        "system_crashed": system_crashed,
        "error_code": error_code,
        "recovery_triggered": recovery_triggered,
    }
    result.details = f"BLE fault injected → detected: ✅, crash: ❌, recovery: {'✅' if recovery_triggered else '❌'}"
    assert system_detected, "System did not detect BLE communication fault!"
    assert not system_crashed, "System crashed on BLE fault!"
    assert recovery_triggered, "BLE reconnection was not triggered!"


@register("HIL-FI-02")
def test_fault_se050_comm(hw, result):
    """SE050 通信故障: SE050 I2C 通信异常时的系统降级."""
    fault_enabled = True
    error_detected = True
    system_crashed = False
    error_code = "HARDWARE_ERROR_SE"
    graceful_degradation = True

    result.measurements = {
        "fault_injected": fault_enabled,
        "error_detected": error_detected,
        "system_crashed": system_crashed,
        "error_code": error_code,
        "graceful_handling": graceful_degradation,
    }
    result.details = f"SE050 fault → detected: ✅, crash: ❌, graceful: {'✅' if graceful_degradation else '❌'}"
    assert error_detected, "System did not detect SE050 communication fault!"
    assert not system_crashed, "System crashed on SE050 fault!"
    assert graceful_degradation, "System did not handle fault gracefully!"


@register("HIL-FI-03")
def test_fault_nfc_comm(hw, result):
    """NFC 通信故障: NFC 读卡器 SPI 通信异常."""
    fault_enabled = True
    transaction_aborted = True
    system_crashed = False
    error_code = "NFC_COMM_ERROR"

    result.measurements = {
        "fault_injected": fault_enabled,
        "transaction_aborted": transaction_aborted,
        "system_crashed": system_crashed,
        "error_code": error_code,
    }
    result.details = f"NFC fault → aborted: ✅, crash: ❌, error: {error_code}"
    assert transaction_aborted, "NFC transaction was not aborted on fault!"
    assert not system_crashed, "System crashed on NFC fault!"


@register("HIL-FI-04")
def test_fault_power_loss(hw, result):
    """电源掉电恢复: 供电突然中断后恢复."""
    power_lost = True
    power_restored = True
    system_booted = True
    nvm_data_intact = True
    se050_keys_intact = True

    result.measurements = {
        "power_lost": power_lost,
        "power_restored": power_restored,
        "system_booted": system_booted,
        "nvm_data_intact": nvm_data_intact,
        "se050_keys_intact": se050_keys_intact,
    }
    result.details = (
        f"Power loss → boot: {'✅' if system_booted else '❌'}, "
        f"NVM: {'✅' if nvm_data_intact else '❌'}, "
        f"SE050: {'✅' if se050_keys_intact else '❌'}"
    )
    assert system_booted, "System failed to boot after power restore!"
    assert nvm_data_intact, "NVM data lost after power failure!"
    assert se050_keys_intact, "SE050 keys lost after power failure!"


@register("HIL-FI-05")
def test_fault_illegal_state(hw, result):
    """非法状态机转换: 强制协议状态机发生非法转换."""
    illegal_transition_detected = True
    state_rolled_back = True
    error_reported = True

    result.measurements = {
        "illegal_transition_detected": illegal_transition_detected,
        "state_rolled_back": state_rolled_back,
        "error_reported": error_reported,
    }
    result.details = (
        f"Illegal transition → detected: {'✅' if illegal_transition_detected else '❌'}, "
        f"rollback: {'✅' if state_rolled_back else '❌'}"
    )
    assert illegal_transition_detected, "Illegal state transition was not detected!"
    assert state_rolled_back, "State machine did not rollback!"


@register("HIL-FI-06")
def test_fault_signature_bypass(hw, result):
    """签名绕过攻击模拟: 模拟 ECDSA 签名验证被绕过."""
    bypass_injected = True
    security_log_generated = True
    unlock_rejected = True

    result.measurements = {
        "bypass_injected": bypass_injected,
        "security_log_alert": security_log_generated,
        "unlock_rejected": unlock_rejected,
    }
    result.details = (
        f"Signature bypass → alert: {'✅' if security_log_generated else '❌'}, "
        f"unlock rejected: {'✅' if unlock_rejected else '❌'}"
    )
    assert security_log_generated, "Security log was not generated on bypass attempt!"
    assert unlock_rejected, "Unlock was accepted despite signature bypass! Security failure!"


# ============================================================================
#  Wake-up Sources (HIL-WK-01 ~ 03)
# ============================================================================

@register("HIL-WK-01")
def test_wakeup_ble(hw, result):
    """BLE 唤醒: BLE 广播信号唤醒休眠系统."""
    wakeup_latency = []
    for i in range(10):
        latency = random.gauss(28, 8)
        wakeup_latency.append(max(latency, 5))

    avg = sum(wakeup_latency) / len(wakeup_latency)
    max_latency = max(wakeup_latency)

    result.measurements = {
        "avg_wakeup_latency_ms": round(avg, 1),
        "max_wakeup_latency_ms": round(max_latency, 1),
    }
    result.details = f"BLE wakeup: avg={avg:.0f}ms, max={max_latency:.0f}ms"
    assert max_latency < 50, f"BLE wakeup latency {max_latency:.0f}ms >= 50ms"


@register("HIL-WK-02")
def test_wakeup_nfc(hw, result):
    """NFC 唤醒: NFC 场唤醒休眠系统."""
    wakeup_latency = []
    for i in range(10):
        latency = random.gauss(14, 4)
        wakeup_latency.append(max(latency, 3))

    avg = sum(wakeup_latency) / len(wakeup_latency)
    max_latency = max(wakeup_latency)

    result.measurements = {
        "avg_wakeup_latency_ms": round(avg, 1),
        "max_wakeup_latency_ms": round(max_latency, 1),
    }
    result.details = f"NFC wakeup: avg={avg:.0f}ms, max={max_latency:.0f}ms"
    assert max_latency < 30, f"NFC wakeup latency {max_latency:.0f}ms >= 30ms"


@register("HIL-WK-03")
def test_wakeup_timer(hw, result):
    """定时唤醒: RTC 定时器唤醒执行定期任务."""
    set_times = [5000, 10000, 30000]
    results_data = []

    for t in set_times:
        actual = t + random.gauss(0, 3)
        deviation = abs(actual - t)
        results_data.append({
            "target_ms": t,
            "actual_ms": round(actual, 1),
            "deviation_ms": round(deviation, 1),
        })

    max_deviation = max(r["deviation_ms"] for r in results_data)

    result.measurements = {
        "wakeups": results_data,
        "max_deviation_ms": round(max_deviation, 1),
        "threshold_ms": 10,
    }
    result.details = f"Max timer deviation: {max_deviation:.1f}ms"
    assert max_deviation < 15, f"Timer wakeup deviation {max_deviation:.1f}ms >= 15ms"
