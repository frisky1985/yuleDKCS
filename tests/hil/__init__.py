"""
yuleDKCS HIL (Hardware-in-the-Loop) test package.

This package contains automated test scripts for validating the yuleDKCS
digital key embedded firmware on real S32K312 EVB + KW47 BLE + ST25R501 NFC
+ SE050 secure element hardware.

Directory structure:
    hil_runner.py       - Main entry point (CLI)
    test_cases.py       - All test case implementations
    test_ble.py         - BLE connection tests module wrapper
    test_nfc.py         - NFC communication tests
    test_uwb.py         - UWB ranging tests
    test_se050.py       - SE050 SCP03 tests
    test_unlock.py      - Unlock response time tests
    test_vehicle_status.py - Vehicle status sync tests
    test_power_mgmt.py  - Power management tests
    test_fault_inject.py - Fault injection tests
    test_wakeup.py      - Wake-up source tests
    flash_hil.jlink     - J-Link flash script
"""

__version__ = "1.0.0"
