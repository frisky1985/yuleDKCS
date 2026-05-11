# BER-TLV CI/CD 流水线配置

> **版本**: v1.0.0  
> **平台**: GitHub Actions  
> **测试范围**: 多语言实现 + 兼容性 + 性能回归

---

## 1. 流水线架构

```
┌────────────────────────────────────────────────────────────────────────┐
│                    TLV CI/CD Pipeline Architecture                │
├────────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Trigger: Push/PR ─────────────────────────────────────────´──────´──────´──────´───´┐   │
│                                                              │   │   │   │   │   │   │
│                                                              ▼   ▼   ▼   ▼   ▼   ▼   │
│  ┌───────────────────────────────────────────────────────────────────────┐               │
│  │                   Stage 1: 代码检查                             │               │
│  ├───────────────────────────────────────────────────────────────────────┤               │
│  │  • Schema验证                                                │               │
│  │  • 代码风格检查 (clang-format, checkstyle, eslint)            │               │
│  │  • 静态分析 (cppcheck, sonarqube)                             │               │
│  └───────────────────────────────────────────────────────────────────────┘               │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐               │
│  │                   Stage 2: 单元测试                           │               │
│  ├───────────────────────────────────────────────────────────────────────┤               │
│  │  • C (cmocka + gcov)                     │              │   │               │
│  │  • Java (JUnit 5 + JaCoCo)               │   并行执行    │   │               │
│  │  • Python (pytest + coverage)            │              │   │               │
│  │  • Swift (XCTest + llvm-cov)             │              │   │               │
│  │  • TypeScript (Jest + istanbul)          │              │   │               │
│  └───────────────────────────────────────────────────────────────────────┘               │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐               │
│  │                   Stage 3: 兼容性测试                       │               │
│  ├───────────────────────────────────────────────────────────────────────┤               │
│  │  • 三端交互测试 (Python测试套件)                          │               │
│  │  • 消息格式验证                                            │               │
│  │  • 版本协商测试                                            │               │
│  └───────────────────────────────────────────────────────────────────────┘               │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐               │
│  │                   Stage 4: 性能测试                         │               │
│  ├───────────────────────────────────────────────────────────────────────┤               │
│  │  • 基准测试 (encode/decode latency)                          │               │
│  │  • 吞吐量测试 (messages/sec)                                 │               │
│  │  • 性能回归检测 (vs 上次构建)                               │               │
│  └───────────────────────────────────────────────────────────────────────┘               │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐               │
│  │                   Stage 5: 安全测试                         │               │
│  ├───────────────────────────────────────────────────────────────────────┤               │
│  │  • Fuzz测试 (30分钟)                                        │               │
│  │  • 密码学验证                                              │               │
│  │  • 秘密扫描 (truffleHog)                                     │               │
│  └───────────────────────────────────────────────────────────────────────┘               │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐               │
│  │                   Stage 6: 发布                             │               │
│  ├───────────────────────────────────────────────────────────────────────┤               │
│  │  • 打包构建物                                             │               │
│  │  • Docker镜像构建                                          │               │
│  │  • 发布到测试环境                                          │               │
│  │  • 货柜发布 (production)                                  │               │
│  └───────────────────────────────────────────────────────────────────────┘               │
│                                                                      │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. GitHub Actions 配置

### 2.1 主流水线配置

```yaml
# .github/workflows/tlv-ci.yml
name: TLV Protocol CI/CD

on:
  push:
    branches: [main, develop]
    paths:
      - 'src/**'
      - 'tests/**'
      - 'schema/**'
      - '.github/workflows/**'
  pull_request:
    branches: [main]

env:
  PROTOCOL_VERSION: "1.2.0"
  RUST_BACKTRACE: 1

jobs:
  # ========== Stage 1: 代码检查 ==========
  lint:
    name: Code Quality Checks
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Schema Validation
        run: |
          pip install pyyaml jsonschema
          python3 scripts/validate_schema.py schema/tlv_schema.yaml

      - name: C Code Formatting
        run: |
          sudo apt-get install -y clang-format
          find src/c -name "*.c" -o -name "*.h" | xargs clang-format --dry-run --Werror

      - name: Java Code Style
        run: |
          cd src/java
          mvn checkstyle:check

      - name: C Static Analysis
        run: |
          sudo apt-get install -y cppcheck
          cppcheck --enable=all --error-exitcode=1 src/c/

  # ========== Stage 2: 单元测试 (并行) ==========
  test-c:
    name: C Unit Tests
    runs-on: ubuntu-latest
    needs: lint
    steps:
      - uses: actions/checkout@v4

      - name: Install Dependencies
        run: sudo apt-get install -y cmake libcmocka-dev gcovr

      - name: Build & Test
        run: |
          cd src/c
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
          make
          ctest --output-on-failure

      - name: Coverage Report
        run: |
          cd src/c/build
          gcovr -r .. --html --html-details -o coverage.html

      - name: Upload Coverage
        uses: actions/upload-artifact@v4
        with:
          name: c-coverage
          path: src/c/build/coverage.html

  test-java:
    name: Java Unit Tests
    runs-on: ubuntu-latest
    needs: lint
    strategy:
      matrix:
        java-version: [11, 17, 21]
    steps:
      - uses: actions/checkout@v4

      - name: Setup Java
        uses: actions/setup-java@v4
        with:
          java-version: ${{ matrix.java-version }}
          distribution: 'temurin'

      - name: Build & Test
        run: |
          cd src/java
          mvn clean test

      - name: Coverage Report
        run: |
          cd src/java
          mvn jacoco:report

      - name: Upload Coverage
        uses: actions/upload-artifact@v4
        with:
          name: java-coverage-${{ matrix.java-version }}
          path: src/java/target/site/jacoco/

  test-python:
    name: Python Unit Tests
    runs-on: ubuntu-latest
    needs: lint
    strategy:
      matrix:
        python-version: ['3.9', '3.10', '3.11', '3.12']
    steps:
      - uses: actions/checkout@v4

      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: ${{ matrix.python-version }}

      - name: Install Dependencies
        run: |
          cd src/python
          pip install -e ".[test]"

      - name: Run Tests
        run: |
          cd src/python
          pytest tests/ -v --cov=tlv_protocol --cov-report=xml

      - name: Upload Coverage
        uses: codecov/codecov-action@v3
        with:
          file: src/python/coverage.xml
          flags: python

  test-swift:
    name: Swift Unit Tests
    runs-on: macos-latest
    needs: lint
    steps:
      - uses: actions/checkout@v4

      - name: Build & Test
        run: |
          cd src/swift
          swift test --enable-code-coverage

      - name: Coverage Report
        run: |
          cd src/swift
          xcrun llvm-cov export -format=lcov .build/debug/*Tests.xctest/Contents/MacOS/*Tests > coverage.lcov

  test-typescript:
    name: TypeScript Unit Tests
    runs-on: ubuntu-latest
    needs: lint
    steps:
      - uses: actions/checkout@v4

      - name: Setup Node
        uses: actions/setup-node@v4
        with:
          node-version: '20'

      - name: Install & Test
        run: |
          cd src/typescript
          npm ci
          npm test -- --coverage

      - name: Upload Coverage
        uses: actions/upload-artifact@v4
        with:
          name: ts-coverage
          path: src/typescript/coverage/

  # ========== Stage 3: 兼容性测试 ==========
  interop-test:
    name: Interoperability Tests
    runs-on: ubuntu-latest
    needs: [test-c, test-java, test-python]
    steps:
      - uses: actions/checkout@v4

      - name: Setup All Environments
        run: |
          # C
          sudo apt-get install -y gcc make
          # Java
          sudo apt-get install -y default-jdk
          # Python
          pip install pyyaml

      - name: Build All Implementations
        run: |
          # Build C encoder/decoder
          cd src/c && make encoder decoder && cd ../..
          # Build Java
          cd src/java && mvn package -DskipTests && cd ../..

      - name: Run Interop Tests
        run: |
          python3 tests/interop/interop_test_suite.py \
            --impl-dir ./implementations \
            --priority P0 \
            --output interop_report.json

      - name: Upload Interop Report
        uses: actions/upload-artifact@v4
        with:
          name: interop-report
          path: interop_report.json

      - name: Check Interop Results
        run: |
          FAILED=$(jq '.summary.failed' interop_report.json)
          if [ "$FAILED" -gt 0 ]; then
            echo "Interop tests failed: $FAILED"
            exit 1
          fi

  # ========== Stage 4: 性能测试 ==========
  performance-test:
    name: Performance Benchmarks
    runs-on: ubuntu-latest
    needs: [test-c, test-java]
    steps:
      - uses: actions/checkout@v4

      - name: Setup
        run: |
          sudo apt-get install -y gcc default-jdk python3-pip
          pip install pyperf

      - name: Build Optimized
        run: |
          cd src/c && make clean && make CFLAGS="-O3 -march=native"

      - name: Run Benchmarks
        run: |
          cd benchmarks
          ./run_benchmarks.sh --output results.json

      - name: Compare with Baseline
        run: |
          python3 benchmarks/compare_with_baseline.py \
            --current results.json \
            --baseline benchmarks/baseline.json \
            --threshold 5  # 5% regression threshold

      - name: Upload Results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: benchmarks/results.json

  # ========== Stage 5: 安全测试 ==========
  security-test:
    name: Security Tests
    runs-on: ubuntu-latest
    needs: lint
    steps:
      - uses: actions/checkout@v4

      - name: Secret Scan
        run: |
          pip install truffleHog
          truffleHog --regex --entropy=False .

      - name: Dependency Scan (Java)
        run: |
          cd src/java
          mvn org.owasp:dependency-check-maven:check

      - name: Fuzz Test (Short)
        run: |
          cd src/c
          make fuzzer
          timeout 1800 ./fuzzer -max_total_time=1800 || true

      - name: Upload Fuzz Results
        uses: actions/upload-artifact@v4
        if: failure()
        with:
          name: fuzz-crash
          path: src/c/crash-*

  # ========== Stage 6: 发布 ==========
  build-release:
    name: Build Release Artifacts
    runs-on: ubuntu-latest
    needs: [interop-test, performance-test, security-test]
    if: github.ref == 'refs/heads/main'
    steps:
      - uses: actions/checkout@v4

      - name: Build C Libraries
        run: |
          cd src/c
          make clean
          # x86_64
          make CFLAGS="-O3 -fPIC" TARGET=libtlv_x86_64.so
          # ARM64 (cross compile)
          make CC=aarch64-linux-gnu-gcc CFLAGS="-O3 -fPIC" TARGET=libtlv_arm64.so

      - name: Build Java Package
        run: |
          cd src/java
          mvn clean package -DskipTests

      - name: Build Python Wheel
        run: |
          cd src/python
          pip install build
          python -m build

      - name: Build Docker Images
        run: |
          docker build -t automotive/tlv-gateway:v${{ env.PROTOCOL_VERSION }} \
            -f docker/Dockerfile.gateway .

      - name: Upload Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: release-artifacts
          path: |
            src/c/libtlv_*.so
            src/java/target/*.jar
            src/python/dist/*.whl

  # ========== 合并报告 ==========
  summary:
    name: CI Summary
    runs-on: ubuntu-latest
    needs: [interop-test, performance-test, security-test]
    if: always()
    steps:
      - name: Download All Artifacts
        uses: actions/download-artifact@v4

      - name: Generate Summary
        run: |
          echo "## TLV Protocol CI Summary" >> $GITHUB_STEP_SUMMARY
          echo "" >> $GITHUB_STEP_SUMMARY
          echo "| Job | Status |" >> $GITHUB_STEP_SUMMARY
          echo "|-----|--------|" >> $GITHUB_STEP_SUMMARY
          echo "| Interop Tests | ${{ needs.interop-test.result }} |" >> $GITHUB_STEP_SUMMARY
          echo "| Performance | ${{ needs.performance-test.result }} |" >> $GITHUB_STEP_SUMMARY
          echo "| Security | ${{ needs.security-test.result }} |" >> $GITHUB_STEP_SUMMARY
```

### 2.2 PR检查流水线

```yaml
# .github/workflows/pr-checks.yml
name: PR Checks

on:
  pull_request:
    types: [opened, synchronize, reopened]

jobs:
  pr-checklist:
    name: PR Checklist
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check PR Description
        uses: actions/github-script@v7
        with:
          script: |
            const body = context.payload.pull_request.body || '';
            const required = [
              '- [ ] 代码测试通过',
              '- [ ] 文档已更新',
              '- [ ] 兼容性测试通过'
            ];
            
            const missing = required.filter(item => !body.includes(item));
            if (missing.length > 0) {
              core.setFailed(`PR描述缺少: ${missing.join(', ')}`);
            }

  size-label:
    name: Size Label
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check PR Size
        run: |
          LINES=$(git diff --stat origin/main | tail -1 | awk '{print $4}')
          if [ $LINES -gt 500 ]; then
            echo "::warning:: PR过大，建议拆分"
          fi
```

---

## 3. 性能回归检测

### 3.1 基准测试脚本

```python
#!/usr/bin/env python3
"""
性能基准测试和回归检查
"""

import json
import subprocess
import sys
import statistics
from dataclasses import dataclass
from typing import List, Dict


@dataclass
class BenchmarkResult:
    name: str
    unit: str
    values: List[float]
    
    @property
    def mean(self) -> float:
        return statistics.mean(self.values)
    
    @property
    def stdev(self) -> float:
        return statistics.stdev(self.values) if len(self.values) > 1 else 0


def run_benchmark(executable: str, args: List[str], iterations: int = 10) -> BenchmarkResult:
    """运行性能测试"""
    values = []
    
    for _ in range(iterations):
        result = subprocess.run(
            [executable] + args,
            capture_output=True,
            text=True
        )
        # 解析输出，提取数值
        value = float(result.stdout.strip())
        values.append(value)
    
    return BenchmarkResult(
        name="latency",
        unit="us",
        values=values
    )


def compare_with_baseline(
    current: Dict[str, BenchmarkResult],
    baseline: Dict[str, BenchmarkResult],
    threshold: float = 5.0
) -> bool:
    """
    比较当前结果与基准
    threshold: 允许的性能回退百分比
    """
    failed = False
    
    for name, current_result in current.items():
        if name not in baseline:
            print(f"⚠️  新测试: {name}")
            continue
        
        baseline_result = baseline[name]
        current_mean = current_result.mean
        baseline_mean = baseline_result.mean
        
        # 计算变化百分比
        change_pct = ((current_mean - baseline_mean) / baseline_mean) * 100
        
        if change_pct > threshold:
            print(f"❌ 性能回退: {name}")
            print(f"   基准: {baseline_mean:.2f} {baseline_result.unit}")
            print(f"   当前: {current_mean:.2f} {current_result.unit}")
            print(f"   变化: +{change_pct:.1f}% (超过阈值 {threshold}%)")
            failed = True
        elif change_pct < -threshold:
            print(f"✅ 性能提升: {name} ({change_pct:.1f}%)")
        else:
            print(f"✅ 稳定: {name} ({change_pct:+.1f}%)")
    
    return not failed


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--current', required=True, help='当前测试结果JSON')
    parser.add_argument('--baseline', required=True, help='基准测试结果JSON')
    parser.add_argument('--threshold', type=float, default=5.0, help='回退阈值%')
    args = parser.parse_args()
    
    # 加载结果
    with open(args.current) as f:
        current_data = json.load(f)
    with open(args.baseline) as f:
        baseline_data = json.load(f)
    
    # 转换为对象
    current = {
        k: BenchmarkResult(**v) for k, v in current_data.items()
    }
    baseline = {
        k: BenchmarkResult(**v) for k, v in baseline_data.items()
    }
    
    # 比较
    passed = compare_with_baseline(current, baseline, args.threshold)
    
    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
```

---

*版本: v1.0.0*  
*最后更新: 2026-05-11*