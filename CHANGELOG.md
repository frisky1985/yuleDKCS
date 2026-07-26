# 变更日志 (Changelog)

所有重要变更均记录在此文件中。

格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本管理遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

---

## [Unreleased]

### Added

- 📱 完成 Android 数字钥匙 SDK 核心模块（Kotlin 实现）
- 📦 yuleDKCS 全流程质量提升 Sprint — 编译修复、测试覆盖率提升、基础设施搭建
- 📋 补全产品需求定稿、模块详细设计、DK Hub/KMS 设计、需求追溯矩阵

### Changed

- 项目文档体系完善，新增多份设计定稿文档

---

## [v2.1.0] — 2026-07-26

> 量产就绪大版本 — 生产级可用

### Added

- 🎯 **yuleDKCS 统一入口**: `cmd/yuledkcs` 支持三种部署模式（all-in-one / hub-only / server-only）
- 🎫 **Token 完整链路**: DK Hub Token 签发/验证/吊销接口 + 权限映射 + 换钥匙接口 + 离线链路
- 🎫 **gRPC 骨架**: 完整的 gRPC 服务定义与三种模式链路实现
- 🏗️ **基础设施搭建**: 编译修复 + 测试覆盖率提升 + 全量推进
- 📱 **端侧 API 测试**: iOS + Android API 客户端全链路测试
- 🚗 **车端测试框架**: C 测试框架 + 前端 UI 测试
- 📱 **多设备配钥**: 设备注册 + 按能力配钥 + 远程吊销
- 🔑 **分享生命周期**: 过期约束 + 接收验证 + 取消/查询
- 🔐 跨 Agent 审查 P0 修复：权限签名 + 竞态条件 + 缺失 API
- 🚀 全流程质量提升 Sprint: 编译修复 + 测试覆盖率 + 文档补全

### Changed

- 📐 **DK Hub 架构定稿**: 三层模型 DK Hub → OEM DK Server → OEM TSP
- 📐 **Hub 架构修正**: 密钥编排层与密钥材料层分离
- 📐 **Hub Token 模型**: Token 签发机模型 + 在线/离线统一
- 📐 **生态服务商场景**: Hub 只做决策不碰钥匙
- 📖 **权限模型文档**: 8 位权限位组合实现多车主场景
- 📚 整理项目结构：审查报告移入 `docs/reviews/`，报告移入 `docs/reports/`
- 🧹 收尾清理：`go.mod` 路径修正 + TODO 清零 + `go.work` 配置
- 🔓 开源准备：Apache 2.0 License + `.gitignore` + 修复硬编码密码
- 🛡️ 渗透 Critical 修复：权限越权 + 速率限制
- 🔒 P0 合规隐患修复：SM2 RNG + CCC GATT + SE050 存储 + 密钥持久化 + ICCOA 引擎检查
- 🔒 P0 漏洞修复闭环：JWT 完整实现 + 速率限制 + RevokeKey 改进
- 🔧 协议标准符合性修复 (Hermes 审查通过)
- 🔧 CR-7~CR-10 低优修复 (Hermes 审查通过)
- 🔧 CR-1~CR-6: Hermes 代码审查发现的修复
- 🔒 安全修复：20 个漏洞全清
- ✅ 回归审查通过：REST gateway handler 所有问题已修复

### Fixed

- 编译错误修复（多层依赖修正）
- Prometheus 指标重复注册问题
- 代码重复声明问题
- 模块导入路径和依赖版本修正
- 9 项 ICCE/ICCOA 对齐问题全部修复
- ICCE(8) + ICCOA(11) 全部 TODO 清零

### Security

- 🔒 渗透测试 Critical 修复：权限越权 + 速率限制绕过
- 🔒 P0 合规隐患修复：SM2 RNG 随机数生成器 + CCC GATT 安全 + SE050 安全存储
- 🔒 JWT 完整实现替换占位逻辑 + 速率限制 + RevokeKey 改进
- 🔒 20 个安全漏洞全部清除
- 🔒 跨 Agent 代码审查 P0 修复

---

## [v2.0.0] — 2026-07-19

> 首个公开发布版本 — ICCE/CCC/ICCOA 三协议完整支持

### Added

- 🏗️ **系统架构设计**: v2.0 Go+Java 混合架构设计定稿
- 🔧 **三端完整代码**: 数字钥匙系统嵌入式 + App + 云端三端完整代码
- 🎛️ **统一协议接口层**: BER-TLV 编解码器 + 统一协议 HAL
- 🌐 **ICCOA 协议栈**: 完成 ICCOA DK 3.0 & DK 4.0 协议实现
- 📡 **BLE 低功耗模式**: NFC LPCD 支持
- 📚 **完善文档体系**: 系统架构设计、API 文档、部署指南、安全指南
- 📱 **Android/iOS SDK**: 原生 SDK 核心模块
- 🛠️ **CI/CD 流水线**: GitHub Actions CI 管道 + Makefile 常用命令
- 🧪 **集成测试框架**: API 字段对齐检查 + 数据转换层
- 🌐 **三端联调指南**: 完整的三端联调基础设施和 API 对接
- 📘 **文档站搭建**: MkDocs + GH Pages 部署
- 🔌 **TSP 适配器**: Java Adapters (Spring Boot) + gRPC Server

### Changed

- 🏗️ **项目结构重构**: 按模块分类，嵌入式/前端/后端/文档清晰分离
- 🧹 仓库清理：删除非技术文档、清理冗余代码、统一 master 到 main
- 📝 **API 文档审计**: 补全 45 个后端路由
- 🎨 **统一 API 错误响应格式**: 所有端点一致的错误码结构
- 🔧 **密钥管理完善**: 数字钥匙管理功能 + 深度完善项目基础设施
- 📚 协议规范文档：数字钥匙数据库协议规范 + 三端通信协议规范
- 🔄 版本分支管理：`master` 主开发 + `legacy-digital-key-project` 归档

### Fixed

- 后端编译错误修复（导入路径、依赖版本、重复声明）
- Prometheus 指标重复注册问题
- 测试修复：key_tracking 全部测试通过

### Security

- SE050 ECDSA 验签 + 密钥导入实现
- mbedtls 集成 + SM2/SM3/SM4 国密模块 + SE050 适配
- ICCE/ICCOA 证书实现 + key_tracking 安全存储

---

## [初始开发] — 2026-05 ~ 2026-06

> 项目启动与核心基础设施搭建阶段

### Added

- 项目初始化：数字钥匙系统三端代码骨架
- Android 数字钥匙 SDK 核心模块
- ICCE/CCC/ICCOA 协议栈初步实现
- 安全模块：SE050 + mbedtls + 国密 SM2/SM3/SM4
- 系统架构设计文档 v1.0

---

## 版本记录

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v2.1.0 | 2026-07-26 | 量产就绪版本：Token 完整链路、Hub 三层架构、全流程质量提升 |
| v2.0.0 | 2026-07-19 | 首个公开发布：三协议栈完成、三端代码完整、文档体系建立 |

[Unreleased]: https://github.com/yule-technology/yuleDKCS/compare/v2.1.0...HEAD
[v2.1.0]: https://github.com/yule-technology/yuleDKCS/compare/v2.0.0...v2.1.0
[v2.0.0]: https://github.com/yule-technology/yuleDKCS/releases/tag/v2.0.0
