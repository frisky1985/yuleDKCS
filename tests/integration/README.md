# yuleDKCS 集成测试 (tests/integration/)

> **过程域**: ASPICE SWE.5 (Software Integration and Integration Test)
> **状态**: ACTIVE | **纳入**: `yuleosh ci run 2`（L2 integration-tests 层）

## 定位

本目录为**根级集成测试层**，覆盖组件间接口契约与数据流（SWE.5.BP3）。分两层：

| 层 | 内容 | 位置 | 触发方式 |
|:---|:-----|:-----|:---------|
| **契约层 (pytest)** | 协议/接口/数据流契约验证（BERTLV 编码、消息类型注册表、Registry 规范化、绑定/分享/控车/状态/心跳数据流），纯 Python 自包含，跨产物校验（与 `include/dk_interfaces.h` 对拍防漂移） | `tests/integration/*.py` | `yuleosh ci run 2` / `pytest tests/integration` |
| **组件层 (Go E2E)** | 真实 HUB + DKCS 服务端到端集成（14 个场景：绑定/无感进入/远程控车/NFC/ICCOA/ICCE 分享/吊销/中继邮箱等） | `backend/cloud/hub/tests/integration/` | `go test -tags=integration ./...`（详见该目录 README） |

两层共用同一组需求追溯标记（`Covers: REQ-xxx`），被 `yuleosh traceability matrix` 自动采集。

## 运行

```bash
# 契约层（CI L2 自动执行）
pytest tests/integration -q

# 组件层（需 hub 二进制或运行中实例）
cd backend/cloud/hub/tests/integration && go test -tags=integration -count=1 -timeout 60s ./...

# 全量
bash tests/integration/run_integration.sh
```

## 需求覆盖

| 测试文件 | 覆盖需求 |
|:---------|:---------|
| `test_protocol_codec.py` | REQ-024 (BERTLV), REQ-018 (签名完整性), REQ-010~017 (消息类型注册表) |
| `test_hub_interfaces.py` | REQ-019/REQ-020 (Registry 规范化), REQ-010/REQ-002 (绑定), REQ-014 (分享), REQ-015 (控车), REQ-016 (状态), REQ-017 (心跳) |

## 变更纪律

- 协议/接口契约变更必须同步更新 `include/` 契约头与本层测试（防漂移校验会失败）。
- 新增需求追溯：在测试函数 docstring 中加 `Covers: REQ-xxx` 行。
