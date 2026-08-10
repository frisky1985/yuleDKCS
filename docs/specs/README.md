# yuleDKCS Specs 目录

## 规范文档索引

| # | 文档 | 描述 | 状态 |
|---|------|------|------|
| 1 | `requirements-index.md` | 完整需求清单 (含ID、状态、位置) | ✅ |
| 2 | `spec-multi-device.md` | 多设备配钥 Spec (已标注 REQ-ID) | ✅ |
| 3 | `spec-fix-p0.md` | P0 修复 Sprint (hub测试/CI门禁/SAST) | ✅ |
| 4 | `spec-fix-kni.md` | KNI 生产 Bug 修复 (registry大小写/空指针) | ✅ |
| 5 | `../../../docs/spec/spec-multi-device.md` | 原始多设备配钥 Spec | ✅ |

## 引用规范

- **RS-xxx**: 系统级需求 (System Requirements)
- **SWR-xxx**: 软件级需求 (Software Requirements)
- 模块分组: DKCS Core / Hub / Protocol / Embedded / Frontend

## 文档维护

- 所有 Spec 需标注 REQ-ID 实现追溯
- 新增需求按编号顺序追加
- 状态更新: PROPOSED → IMPLEMENTED → VERIFIED → DEPRECATED
