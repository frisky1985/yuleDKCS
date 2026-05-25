# yuleDKCS 项目完善计划 v2

> 基于完整性检查重新评估 (2026-05-25)

## 重新评估

| 模块 | 代码量 | 状态 |
|------|--------|------|
| frontend/ | 5,107 行 TypeScript | ✅ 8页面+API层+路由+测试 |
| mobile/ | 9,808 行 Swift/Kotlin/Dart | ✅ iOS SDK+Android SDK+Flutter |

**缺口分析**: 代码已有，缺文档和部分功能完善。

---

## 任务清单（更新版）

### ✅ T1: .gitignore — 已完成

### T2: frontend 文档补全 + 车辆管理页

**缺口**:
- ❌ 无 README.md
- ❌ 无车辆管理页面 (vehicle list/detail)
- ❌ 无 API 接口文档

**步骤**:
1. 创建 `frontend/API.md` — 前端 API 接口文档
2. 创建 `frontend/README.md` — 项目说明
3. 设计车辆管理页接口 → 创建 VehiclePage

### T3: mobile 文档补全

**缺口**:
- ❌ 无 `mobile/ios/README.md`
- ❌ 各 SDK 缺少使用文档

**步骤**:
1. 创建 `mobile/ios/README.md` — iOS SDK 集成文档
2. 创建 `mobile/android/README.md` (已有，检查完整性)
3. 创建 `mobile/flutter/README.md` — Flutter 集成文档

### T4: HAL 层单元测试

**位置**: `embedded/src/hal/uwb/dw3000_driver.c`
**现状**: SPI 读写函数为空存根；无测试
**步骤**:
1. 编写 SPI mock 接口
2. 创建 `tests/unit/test_hal_uwb.c`
3. 覆盖: init、configure_sts、set_channel、start_ranging

### T5: DW3000 SPI 驱动

**现状**: `dw3000_read_reg()` 和 `dw3000_write_reg()` 构建头部但不调用 SPI
**方案**: 定义 SPI 总线接口结构体，解耦具体实现
**步骤**:
1. 设计文档: SPI HAL 接口定义
2. 实现 SPI 总线接口 + 占位实现
3. 更新 dw3000_driver 使用新接口
