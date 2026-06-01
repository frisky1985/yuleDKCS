# 🌍 yuleDKCS Community

> Open Core Digital Key Platform — Community Edition

Welcome to the yuleDKCS Community! This is where developers, OEMs, Tier-1 suppliers, and digital key enthusiasts collaborate on the world's most open digital key implementation.

---

## 📖 What's Open vs Enterprise

yuleDKCS follows an **Open Core** model:

### ✅ Community Edition (Apache 2.0 — fully open source)

| Module | Description |
|--------|-------------|
| `embedded/icce_protocol/` | ICCE protocol stack (BLE/UWB/Security) |
| `embedded/ccc_protocol/` | CCC Digital Key 3.0 protocol stack |
| `embedded/iccoa_protocol/` | ICCOA DK 3.0 & 4.0 protocol stack |
| `embedded/unified_protocol/` | Unified protocol abstraction layer |
| `embedded/test_suite/` | Comprehensive test suite |
| `frontend/android/` | Android SDK (Kotlin) |
| `frontend/ios/` | iOS SDK (Swift) |
| `backend/cloud/hub/` | Hub routing service (Go) |
| `backend/cloud/protocol/` | Protocol specifications & proto files |
| `docs/` | Architecture, API, Security docs |

### 🔒 Enterprise Edition (proprietary)

| Module | Description |
|--------|-------------|
| `backend/adapters/` | TSP-specific adapters (CCC, ICCOA, ICCE) |
| `backend/dkcs/` | DKCS core service (key lifecycle management) |
| `backend/cloud/db/` | Database schema & migration scripts |
| `backend/cloud/deploy/` | Production deployment configurations |
| `frontend/android-app/` | Reference Android app implementation |
| `frontend/ios-app/` | Reference iOS app implementation |

---

## 🚀 Getting Started

### Prerequisites

- **Embedded**: CMake ≥ 3.20, ARM GCC toolchain, NXP SDK
- **Android**: Android Studio, Gradle, Kotlin 1.9+
- **iOS**: Xcode 15+, Swift 5.9+
- **Hub**: Go 1.22+

### Quick Start

```bash
# Clone the repository
git clone https://github.com/yuledkcs/yuleDKCS.git
cd yuleDKCS

# Build the embedded protocol stack
cd embedded/icce_protocol
mkdir build && cd build
cmake .. && make

# Build the Android SDK
cd ../../frontend/android
./gradlew build

# Build the iOS SDK
cd ../ios
xcodegen generate
xcodebuild -scheme DigitalKeySDK

# Start the Hub service
cd ../../backend/cloud/hub
go run cmd/hub/main.go
```

For detailed setup, see [README.md](README.md).

---

## 🤝 How to Contribute

We welcome contributions from individuals and organizations!

### Ways to Contribute

- **🐛 Report bugs**: Open a GitHub Issue with reproduction steps
- **💡 Suggest features**: Use the Feature Request template
- **🛠️ Submit code**: Follow the [contribution workflow](CONTRIBUTING.md)
- **📝 Improve docs**: Fix typos, add examples, translate
- **🔬 Review code**: Comment on open pull requests
- **🧪 Test**: Run the test suite and report edge cases

### Contribution Opportunities

Check our [Good First Issues](https://github.com/yuledkcs/yuleDKCS/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) and [Help Wanted](https://github.com/yuledkcs/yuleDKCS/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22) labels.

---

## 📋 Community Guidelines

### Code of Conduct

All community members must follow our [Code of Conduct](CODE_OF_CONDUCT.md). We are committed to providing a welcoming, inclusive, and harassment-free environment.

### Communication Channels

| Channel | Purpose |
|---------|---------|
| [GitHub Issues](https://github.com/yuledkcs/yuleDKCS/issues) | Bug reports, feature requests |
| [GitHub Discussions](https://github.com/yuledkcs/yuleDKCS/discussions) | General questions, ideas |
| [Community Discord](https://discord.gg/yuledkcs) | Real-time chat, support |
| [Mailing List](https://groups.google.com/g/yuledkcs-dev) | Release announcements, RFCs |

### RFC Process

For significant architectural changes:
1. Write an RFC as a GitHub Discussion
2. Community review period: **2 weeks minimum**
3. Core maintainers vote and finalize
4. RFC is implemented via normal PR workflow

---

## 🏗️ Governance

### Maintainers

The project is maintained by the **yuleDKCS Core Team**. Current maintainers are listed in [MAINTAINERS.md](MAINTAINERS.md).

### Decision Making

- **Minor changes**: PR approval by any maintainer
- **Major changes**: PR approval by 2+ maintainers, with RFC review
- **License/Governance changes**: Community vote (simple majority)

### Contributor Ladder

1. **Contributor**: Anyone who submits a merged PR
2. **Regular Contributor**: 5+ merged PRs, active in discussions
3. **Maintainer**: Nominated by existing maintainers, voted by community
4. **Core Maintainer**: Long-standing maintainer with architectural oversight

---

## 🎉 Community Events

- **Monthly Community Call**: First Wednesday of each month (rotate EN/CN)
- **Annual yuleDKCS Summit**: Technical talks, workshops, roadmap planning
- **Hackathons**: Periodic digital key themed hackathons

Check our [Community Calendar](https://calendar.google.com/calendar/u/0?cid=yuledkcs.com) for upcoming events.

---

## 📄 License

[![Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)

The yuleDKCS Community Edition is licensed under the **Apache License, Version 2.0**. See [LICENSE](LICENSE) for the full text.

The yuleDKCS Enterprise Edition is available under a commercial license. Contact [enterprise@yuledkcs.com](mailto:enterprise@yuledkcs.com) for details.

---

## 💬 Get in Touch

- **Community Support**: GitHub Discussions or Discord
- **Enterprise Inquiries**: [enterprise@yuledkcs.com](mailto:enterprise@yuledkcs.com)
- **Security Reports**: [security@yuledkcs.com](mailto:security@yuledkcs.com) (see [SECURITY.md](SECURITY.md))
- **Twitter/X**: [@yuleDKCS](https://twitter.com/yuledkcs)

---

*Built with ❤️ by the yuleDKCS community*
