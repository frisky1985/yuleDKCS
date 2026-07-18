# CMD 入口函数测试

## 背景

DKCS/Hub/YuleDKCS 三个入口是生产服务起点，任意 panic 导致服务不可用。当前 cmd 包覆盖率均为 0%。

## 需求

### SWR-001: dkcs 入口测试

**SHALL** dkcs cmd 包测试覆盖 `initDatabase` 和 `initRedis` 两个导出函数，验证配置错误时返回对应 error、无效配置不 panic。

**Reason**: 生产入口的依赖初始化过程是第一个故障点，数据库/Redis 连接失败必须被正确传播而非静默吞掉。

**Status**: DRAFT

### SWR-002: hub 入口测试

**SHALL** hub cmd 包通过提取 `setupHubGRPCServer` 函数进行模块化测试，验证适配器注册完整（6 个适配器）、各 Service 创建不 panic、Keepalive 参数配置正确。

**Reason**: Hub 依赖 6 个车厂适配器注册流程，注册遗漏会导致线上密钥管理故障。

**Status**: DRAFT

### SWR-003: yuledkcs 统一入口测试

**SHALL** yuledkcs cmd 包测试三种启动模式（all-in-one / hub-only / server-only）的路由逻辑，验证 flag 解析正确性、各模式启动函数不 panic、无效模式优雅降级到默认值。

**Reason**: 统一入口是混合部署的关键路径，模式路由错误会导致启动失败或非预期部署拓扑。

**Status**: DRAFT
