# yuleDKCS API 对齐与修复计划

> **日期**: 2026-05-26
> **来源**: API 对齐报告 (`docs/api/api_alignment_report.md`)

---

## 优先级分类

```
P0 — 功能不可用 (3项)
├── 后端 Context Key 不一致 (key_handler.go: user_id → userID)
├── OTA 路由未注册 (router.go)
└── 分享钥匙参数不匹配 (shared_to_username → user_id)

P1 — 核心功能缺失 (3项)
├── iOS SDK: 补充9个API端点 (车辆/钥匙/OTA/WS)
├── Android SDK: 补充5个API端点 (车辆/命令)
└── 车辆 UpdateLocation/Heartbeat 路由注册

P2 — 体验增强 (3项)
├── 后端错误格式统一 ({code,message,data})
├── WebSocket `/ws` 占位符完善
└── GetProfile 手动转换优化
```

---

## 执行计划

| 批次 | 任务 | Agent | 预计时间 |
|:---:|:---|:---:|:---:|
| **1** | P0: 后端Context Key + OTA路由 + 分享参数 | 1 | 10min |
| **1** | P1: iOS SDK API补充 | 2 | 30min |
| **1** | P1: Android SDK API补充 | 3 | 20min |
| **2** | P2: 错误格式 + WS + Vehicle路由 | 1 | 15min |
| **2** | 文档更新: API_REFERENCE.md | 2 | 15min |
| **2** | 任务状态更新: TASK_STATUS.md | 3 | 5min |
