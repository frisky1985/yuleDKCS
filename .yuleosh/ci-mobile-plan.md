# yuleDKCS 移动端 CI 计划 (Mobile CI Plan)

> 版本: 1.0.0 | 日期: 2026-07-07
> 目的: 为 Android 和 iOS SDK + App 提供自动化 CI 骨架
> 对齐: ASPICE SWE.4 (软件单元验证) + SWE.5 (软件集成与测试)

---

## 1. Android CI (Kotlin/Gradle)

### 1.1 Job 定义 (GitHub Actions)

```yaml
# .github/workflows/android-ci.yml
name: Android CI

on:
  push:
    branches: [main, develop, release/*]
    paths:
      - 'frontend/android/**'
  pull_request:
    paths:
      - 'frontend/android/**'

jobs:
  lint:
    name: Lint
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: frontend/android
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up JDK 17
        uses: actions/setup-java@v4
        with:
          java-version: '17'
          distribution: 'temurin'
      
      - name: Cache Gradle
        uses: actions/cache@v4
        with:
          path: |
            ~/.gradle/caches
            ~/.gradle/wrapper
          key: ${{ runner.os }}-gradle-${{ hashFiles('**/*.gradle*', '**/gradle-wrapper.properties') }}
      
      - name: Run detekt (lint)
        run: ./gradlew detekt
      
      - name: Save lint report
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: android-lint-report
          path: frontend/android/build/reports/detekt/

  test:
    name: Test
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: frontend/android
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up JDK 17
        uses: actions/setup-java@v4
        with:
          java-version: '17'
          distribution: 'temurin'
      
      - name: Run unit tests
        run: ./gradlew testDebugUnitTest
      
      - name: Run instrumented tests (with emulator)
        uses: reactivecircus/android-emulator-runner@v2
        with:
          api-level: 31
          target: google_apis
          arch: x86_64
          script: ./gradlew connectedDebugAndroidTest
      
      - name: Save test reports
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: android-test-reports
          path: |
            frontend/android/app/build/reports/tests/
            frontend/android/app/build/reports/androidTests/

  coverage:
    name: Coverage
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: frontend/android
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up JDK 17
        uses: actions/setup-java@v4
        with:
          java-version: '17'
          distribution: 'temurin'
      
      - name: Run tests with coverage
        run: ./gradlew jacocoTestReport
      
      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v4
        with:
          directory: frontend/android/app/build/reports/jacoco/
          flags: android
          name: android-coverage
      
      - name: Save coverage report
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: android-coverage-report
          path: frontend/android/app/build/reports/jacoco/
```

### 1.2 工具链

| 工具 | 用途 | 配置 |
|:-----|:-----|:-----|
| **detekt** | Kotlin 静态分析 | `frontend/android/config/detekt.yml` |
| **JUnit 5** | 单元测试框架 | `testImplementation("org.junit.jupiter:junit-jupiter:5.10.0")` |
| **MockK** | Kotlin Mock 框架 | `testImplementation("io.mockk:mockk:1.13.8")` |
| **Jacoco** | 代码覆盖率 | `frontend/android/app/build.gradle.kts` 中集成 |
| **Robolectric** | Android 单元测试模拟 | `testImplementation("org.robolectric:robolectric:4.11")` |

### 1.3 覆盖率门禁

- 新代码: ≥ 85%
- 总项目: ≥ 70%
- SDK 公共 API: 100%（接口方法必须测试）
- 门禁失败: CI 阻断

---

## 2. iOS CI (Swift/Xcode)

### 2.1 Job 定义 (GitHub Actions)

```yaml
# .github/workflows/ios-ci.yml
name: iOS CI

on:
  push:
    branches: [main, develop, release/*]
    paths:
      - 'frontend/ios/**'
  pull_request:
    paths:
      - 'frontend/ios/**'

jobs:
  lint:
    name: SwiftLint
    runs-on: macos-14
    defaults:
      run:
        working-directory: frontend/ios
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Install SwiftLint
        run: brew install swiftlint
      
      - name: Run SwiftLint
        run: swiftlint --strict
      
      - name: Save lint report
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: ios-lint-report
          path: frontend/ios/swiftlint-report.html

  test:
    name: Test
    runs-on: macos-14
    defaults:
      run:
        working-directory: frontend/ios
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Select Xcode version
        run: sudo xcode-select -s /Applications/Xcode_15.3.app/Contents/Developer
      
      - name: Resolve dependencies
        run: xcodebuild -resolvePackageDependencies -workspace yuleDKCS.xcworkspace -scheme yuleDKCS
      
      - name: Run unit tests
        run: |
          xcodebuild test \
            -workspace yuleDKCS.xcworkspace \
            -scheme yuleDKCS \
            -sdk iphonesimulator \
            -destination 'platform=iOS Simulator,name=iPhone 15 Pro,OS=17.4' \
            -enableCodeCoverage YES \
            -resultBundlePath TestResults
      
      - name: Convert xcresult to HTML
        run: |
          xcrun xccov view --report TestResults.xcresult
      
      - name: Save test results
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: ios-test-results
          path: TestResults.xcresult
```

### 2.2 工具链

| 工具 | 用途 | 配置 |
|:-----|:-----|:-----|
| **SwiftLint** | Swift 代码风格检查 | `frontend/ios/.swiftlint.yml` |
| **XCTest** | Apple 原生测试框架 | 内置于 Xcode |
| **Quick/Nimble** | BDD 测试框架（可选） | `Podfile` 或 SPM 集成 |
| **xcov** | Xcode 覆盖率报告 | Homebrew: `brew install xcov` |

### 2.3 覆盖率门禁

- 新代码: ≥ 80%
- SDK 公共 API: 100%
- 门禁失败: CI 阻断

---

## 3. 与 yuleOSH 证据链集成

```yaml
# .yuleosh/ci-pipeline.yaml 扩展
stages:
  - name: layer4-mobile-android
    label: "L4: Android SDK CI"
    commands:
      - cd frontend/android && ./gradlew detekt testDebugUnitTest jacocoTestReport
    evidence:
      - report: android-lint
      - report: android-coverage

  - name: layer5-mobile-ios
    label: "L5: iOS SDK CI"
    commands:
      - cd frontend/ios && swiftlint && xcodebuild test ...
    evidence:
      - report: swiftlint-results
      - report: ios-coverage
```

---

## 4. 本地开发验证

```bash
# Android
cd frontend/android
./gradlew detekt                              # lint
./gradlew testDebugUnitTest                   # unit test
./gradlew jacocoTestReport                    # coverage

# iOS
cd frontend/ios
swiftlint                                      # lint
xcodebuild test -scheme yuleDKCS -sdk iphonesimulator  # unit test
```

---

## 5. 实施计划

| 阶段 | 内容 | 责任人 | 时间 |
|:-----|:------|:-------|:-----|
| Phase 1 | Android lint + test + coverage CI | TBD | S1 |
| Phase 2 | iOS lint + test CI | TBD | S2 |
| Phase 3 | 与 yuleOSH 证据链对接 | TBD | S3 |
| Phase 4 | CI 门禁 + 覆盖率跟踪 | TBD | S4 |
