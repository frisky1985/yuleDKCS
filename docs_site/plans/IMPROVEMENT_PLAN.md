# yuleDKCS 项目完善计划

> 文档版本: v1.0
> 基于完整性检查报告 (2026-05-25)
> 文档先行 — 请确认后再执行

## 概述

基于完整性检查发现的 5 项缺口，制定以下完善计划。所有改动遵循**文档先行**原则。

---

## 任务清单

### T1: 创建 .gitignore

**位置**: 项目根目录 `/home/admin/yuleDKCS/.gitignore`

**原因**: 缺少 `.gitignore`，可能导致编译产物、凭证文件等被误提交

**内容**:
```gitignore
# 编译产物
*.o
*.a
*.out
/build/

# IDE
.vscode/
.idea/
*.swp

# 环境变量
.env
.env.local
*.pem
*.key

# Go
backend/vendor/
backend/bin/

# Node
node_modules/
dist/

# Python
__pycache__/
*.pyc
```

**预估**: 5 分钟 · 难度: ★☆☆☆☆

---

### T2: 完善 frontend 骨架 → MVP 实现

**位置**: `/home/admin/yuleDKCS/frontend/`

**现状**: React + Vite + TypeScript 框架已搭建，但为模板代码

**建议范围** (文档先行 — 仅确定范围和接口，不实现全部UI):
- ✅ 确定前端与 backend API 的接口协议
- ✅ 创建 API 客户端层 (api/client.ts) 
- ✅ 实现核心页面: 钥匙列表、车辆列表、仪表盘
- ✅ 路由配置

**依赖**: 需要先 review backend API 接口文档

**预估**: 需要先确认范围 · 难度: ★★★☆☆

---

### T3: 完善 mobile 骨架 → 基础功能

**位置**: `/home/admin/yuleDKCS/mobile/`

**现状**: iOS/Android/Flutter 目录已创建，但为模板代码

**建议范围**:
- ✅ iOS SDK: BLE 连接+基础 API 封装
- ✅ Android SDK: BLE 连接+基础 API 封装
- ✅ Flutter App: 跨平台 UI 框架 + BLE 桥接

**依赖**: 需要与 backend API 和 embedded BLE 协议对齐

**预估**: 需要先确认范围 · 难度: ★★★★☆

---

### T4: HAL 层单元测试

**位置**: `/home/admin/yuleDKCS/embedded/tests/unit/test_hal_uwb.c`

**现状**: `embedded/src/hal/uwb/dw3000_driver.c` 有代码，但无测试覆盖

**测试范围**:
```c
// 测试 DW3000 驱动核心函数:
// - dw3000_init_impl()        → SPI配置、中断配置
// - dw3000_configure_sts()    → STS模式配置
// - dw3000_set_channel()      → 信道配置
// - dw3000_start_ranging()    → 测距启动
// - dw3000_read_reg()         → SPI寄存器读取 (空存根)
// - dw3000_write_reg()        → SPI寄存器写入 (空存根)
```

**注意**: 读写寄存器的 SPI 调用是空存根，测试需 mock SPI 层

**预估**: 1-2 小时 · 难度: ★★☆☆☆

---

### T5: DW3000 SPI 驱动实现

**位置**: `/home/admin/yuleDKCS/embedded/src/hal/uwb/dw3000_driver.c`

**现状**: `dw3000_read_reg()` 和 `dw3000_write_reg()` 构建了 SPI 头部但从未执行 SPI 总线调用

**依赖**: NXP SDK SPI 驱动 (`fsl_spi.h` / `fsl_dspi.h`)

**路径** (两种方案):

| 方案 | 方法 | 难度 | 
|------|------|------|
| A | 通过 NXP SDK `SPI_MasterTransferBlocking()` 实现 | 需 SDK |
| B | 通过 HAL 抽象层回调注册 (松耦合) | 中等 |

**推荐方案B** — 定义 SPI 接口结构体，注入具体实现：
```c
typedef struct {
    int (*read)(uint8_t *data, size_t len);
    int (*write)(const uint8_t *data, size_t len);
    int (*transfer)(const uint8_t *tx, uint8_t *rx, size_t len);
} dw3000_spi_bus_t;
```

这样不直接依赖 NXP SDK，后续可灵活切换。

**预估**: 需要确认方案 · 难度: ★★☆☆☆

---

## 优先级建议

| 优先级 | 任务 | 理由 | 推荐执行顺序 |
|--------|------|------|-------------|
| 🔴 P0 | T1: .gitignore | 基础工程规范，无依赖 | 1st |
| 🟠 P1 | T4: HAL 测试 | 代码质量，无依赖 | 2nd |
| 🟠 P1 | T5: SPI 驱动 | 功能完整性，无外部依赖 | 3rd |
| 🟡 P2 | T2: frontend | 前端骨架填充，依赖 API 设计 | 4th |
| 🟡 P2 | T3: mobile | 移动端骨架填充，依赖协议对齐 | 5th |

---

## 约束条件

1. **文档先行** — 每个任务先出设计文档/API规范，确认后再编码
2. **测试覆盖** — 每个功能模块必须配套单元测试 (≥80%)
3. **不阻塞** — T2/T3 需要先确认接口范围；T5 需确认 SPI 方案
4. **跳过密码学存根** — SM3/SM2/SE050 存根本轮已替换完成

---

请教主确认：
1. T4、T5 是否优先执行？
2. T2 (frontend) 当前是否有必要填充？还是保持骨架等后续？
3. T3 (mobile) 同理
