# 🤝 Contributing to yuleDKCS

Thank you for your interest in contributing to yuleDKCS! This guide covers the contribution workflow, coding standards, and best practices.

---

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Pull Request Process](#pull-request-process)
- [Testing Requirements](#testing-requirements)
- [Documentation](#documentation)
- [Spec Delta 流程](#-spec-delta--变更管理流程)
- [Security](#security)
- [Community](#community)

---

## Code of Conduct

All contributors must adhere to our [Code of Conduct](CODE_OF_CONDUCT.md). Be respectful, inclusive, and constructive.

---

## Getting Started

### 1. Fork & Clone

```bash
# Fork the repository on GitHub, then:
git clone https://github.com/YOUR-USERNAME/yuleDKCS.git
cd yuleDKCS
git remote add upstream https://github.com/yuledkcs/yuleDKCS.git
```

### 2. Set Up Development Environment

See [README.md](README.md) for platform-specific setup instructions.

### 3. Pick an Issue

Look for issues labeled:
- `good first issue` — for newcomers
- `help wanted` — actively seeking contributors
- `bug` — confirmed bugs
- `enhancement` — feature requests

Always comment on the issue before starting work to avoid duplication.

---

## Development Workflow

### Branch Naming

| Pattern | Example | Purpose |
|---------|---------|---------|
| `feat/<module>-<description>` | `feat/icce-uwb-ranging` | New feature |
| `fix/<module>-<description>` | `fix/hub-connection-timeout` | Bug fix |
| `docs/<description>` | `docs/api-cleanup` | Documentation |
| `refactor/<module>-<description>` | `refactor/bertlv-decoder` | Code restructuring |
| `test/<module>-<description>` | `test/iccoa-dk4` | Test additions |
| `chore/<description>` | `chore/ci-setup` | Maintenance |

### Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary>

[optional body]
[optional footer]
```

| Type | Usage |
|------|-------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation |
| `refactor` | Code restructuring (no behavior change) |
| `test` | Adding/updating tests |
| `chore` | Build, CI, dependencies |
| `perf` | Performance improvement |
| `security` | Security fix |

**Examples:**

```
feat(icce): add UWB secure ranging for ICCE protocol
fix(hub): fix WebSocket reconnection backoff logic
docs(api): update REST endpoint documentation
refactor(bertlv): extract tag parsing into separate module
```

### Keep Commits Atomic

Each commit should be a single logical change. Split large features into multiple commits.

---

## Coding Standards

### General Principles

- **Readability over cleverness** — write code for humans first
- **Defensive programming** — validate inputs, handle errors
- **No hardcoded secrets** — use environment variables or config files
- **Consistent style** — follow the language-specific conventions

### C (Embedded — `embedded/`)

- **Style**: [MISRA C:2012](https://www.misra.org.uk/) compliant where practical
- **Naming**: `snake_case` for functions/variables, `SCREAMING_SNAKE_CASE` for macros
- **Headers**: Include guards with `#pragma once`
- **Indentation**: 4 spaces (no tabs)
- **Line length**: 100 characters max
- **Comments**: Doxygen-style for public APIs (`/// <summary>`)
- **Error handling**: Return error codes via enum (`int32_t`), check everywhere
- **Memory**: Stack allocation preferred; heap must document ownership

### Go (Backend — `backend/cloud/hub/`)

- **Style**: `gofmt` + `go vet` + `golangci-lint` required
- **Naming**: `camelCase` for unexported, `PascalCase` for exported
- **Error handling**: Always check errors; wrap with context
- **Logging**: Use structured logging (zap logger)
- **Concurrency**: Document goroutine lifecycle; avoid leaks
- **Tests**: Table-driven tests preferred

### Kotlin (Android — `frontend/android/`)

- **Style**: [Kotlin Coding Conventions](https://kotlinlang.org/docs/coding-conventions.html)
- **Format**: Spotless with ktlint
- **Coroutines**: Proper scope management (viewModelScope, lifecycleScope)
- **Null safety**: Prefer `val` over `var`, avoid `!!`

### Swift (iOS — `frontend/ios/`)

- **Style**: [Swift API Design Guidelines](https://www.swift.org/documentation/api-design-guidelines/)
- **Format**: SwiftFormat with provided `.swiftformat` config
- **Access control**: Use `private`/`fileprivate` aggressively
- **Memory**: Watch for retain cycles with closures

### Java (TSP Adapters — `backend/adapters/`)

> **Note**: The adapters module is part of the **Enterprise Edition** and follows a separate set of guidelines available internally.

- **Style**: [Google Java Style Guide](https://google.github.io/styleguide/javaguide.html)
- **Build**: Maven with standard lifecycle
- **Error handling**: Use CompletableFuture for async operations

### Documentation

- **API docs**: OpenAPI 3.0 for REST, protobuf comments for gRPC
- **Code comments**: Explain *why*, not *what*
- **Markdown**: Use `markdownlint` for consistency

---

## Pull Request Process

### PR Checklist

Before submitting a PR, ensure:

- [ ] Code follows the [coding standards](#coding-standards)
- [ ] Tests pass (`./scripts/run-tests.sh`)
- [ ] New code has adequate test coverage (≥80%)
- [ ] Documentation updated if applicable
- [ ] Commit messages follow [conventional commits](#commit-messages)
- [ ] Branch is rebased on latest `main`
- [ ] No sensitive data (API keys, credentials, internal URLs)
- [ ] CHANGELOG updated (if applicable)

### PR Title & Description

```markdown
## Title
<type>(<scope>): <concise description>

## Description
### What does this PR do?
<clear explanation of the change>

### Why is this needed?
<context, issue reference>

### Testing
- [ ] Unit tests added/passed
- [ ] Integration tests passed
- [ ] Manual testing performed

### Related Issues
Closes #<issue-number>

### Breaking Changes
<yes/no — if yes, describe migration path>
```

### PR Size Guidelines

- **Small PRs preferred** — keep under 400 lines changed
- **Large changes** — discuss with maintainers first; split into logical PRs
- **Squash merge** — all commits in a PR will be squashed

### Review Process

```
Submit PR → CI Checks → 1 Maintainer Review → 2 Approvals → Merge
```

1. **CI must pass** — lint, test, build all green
2. **At least 1 maintainer review** required
3. **2 approvals** for significant changes
4. **Changes requested** — address feedback; do *not* resolve conversations yourself
5. **Merge** — maintainers squash-merge into `main`

### Review Etiquette

- **Reviewers**: Be constructive, specific, and kind
- **Authors**: Respond within 5 business days; close stale PRs

---

## Testing Requirements

### All Layers

- **Unit tests**: Required for all new code
- **Integration tests**: Required for cross-module changes
- **No regression allowed** — test suite must stay green

### Coverage Targets

| Module | Coverage Target |
|--------|----------------|
| Embedded (C) | ≥ 80% line coverage |
| Hub (Go) | ≥ 85% line coverage |
| Android SDK (Kotlin) | ≥ 75% line coverage |
| iOS SDK (Swift) | ≥ 75% line coverage |

### Running Tests

```bash
# Embedded tests
cd embedded/icce_protocol && mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON && make && ctest

# Hub tests
cd backend/cloud/hub && go test ./...

# Android SDK tests
cd frontend/android && ./gradlew test

# iOS SDK tests
cd frontend/ios && xcodebuild test -scheme DigitalKeySDK
```

---

## Documentation

### Where to Document

| What | Where |
|------|-------|
| API reference | `docs/API_REFERENCE.md` |
| Architecture | `docs/SYSTEM_ARCHITECTURE.md` |
| Security white paper | `docs/SECURITY_WHITEPAPER.md` |
| Deployment guide | `docs/DEPLOYMENT_GUIDE.md` |
| Code comments | Inline in source files |
| Changelog | `CHANGELOG.md` |
| README | `README.md` (keep concise) |

### Documentation Standards

- Written in Markdown
- Use Mermaid diagrams for architecture
- Include Chinese and English versions for key documents
- Keep examples runnable and copy-paste friendly

---

## Security

### Reporting Vulnerabilities

**DO NOT** file public issues for security vulnerabilities. See [SECURITY.md](SECURITY.md) for the disclosure process.

### Security Best Practices

- Never commit secrets, API keys, or credentials
- Use environment variables or vault services for configuration
- Run `gitleaks` or `trufflehog` locally before pushing
- Follow principle of least privilege in code
- Validate and sanitize all external inputs

---

## Community

### Communication

- **GitHub Issues** — bug reports and feature requests
- **GitHub Discussions** — questions and ideas
- **Discord** — real-time chat (invite link in README)

### Recognition

All contributors get listed in:
- [CONTRIBUTORS.md](CONTRIBUTORS.md) (alphabetical by username)
- Release notes for the version they contributed to
- Annual yuleDKCS contributor spotlight

---

## 📋 Spec Delta & 变更管理流程

所有代码变更（包括 bug fix、refactor、feature）必须附带 **Spec Delta**，确保变更可追溯、可验证。

### 核心要求

- 每个 PR 必须包含 Spec Delta（内联或独立文件）
- Spec Delta 模板见 [`docs/spec-delta-template.md`](docs/spec-delta-template.md)
- 完整的变更管理流程见 [`docs/CHANGE_PROCESS.md`](docs/CHANGE_PROCESS.md)

### 快速开始

1. 复制 [`docs/spec-delta-template.md`](docs/spec-delta-template.md) 中的模板
2. 填写变更范围、影响模块、变更类型、受影响需求
3. 将模板内容放入 PR 描述或保存为 `docs/spec/deltas/YYYY-MM-DD-<brief>.md`
4. 更新 CHANGELOG 的 `[Unreleased]` 章节

### PR Checklist 补充项

- [ ] Spec Delta 已填写并与变更一致
- [ ] 受影响需求可在 `docs/requirement-traceability-matrix.md` 中找到
- [ ] CHANGELOG 已更新

---

## 🚦 CI/CD

Our CI pipeline runs on GitHub Actions:

```
Lint → Unit Tests → Integration Tests → Build → Security Scan
```

- **Commit hooks**: We recommend `pre-commit` for local checks
- **DCO**: All commits must be signed off (`git commit -s`)

---

## FAQ

**Q**: I'm new to automotive security. Can I still contribute?
**A**: Yes! We welcome contributions in documentation, testing, translation, and beginner-friendly code issues.

**Q**: Can I use yuleDKCS in my commercial product?
**A**: The Community Edition (Apache 2.0) can be freely used. Contact us for Enterprise Edition licensing.

**Q**: How long does review typically take?
**A**: Small PRs: 1-3 days. Large PRs: 1-2 weeks.

---

*Thank you for making yuleDKCS better! 🚗*
