# P0-04 修复记录: SwiftLint 启用 force_cast / force_try

## 问题
`.swiftlint.yml` 的 `disabled_rules` 中包含了 `force_cast` 和 `force_try`，导致 ASIL-B(D) 安全关键代码中的强制类型转换/强制 try 不会被阻止。

## 修复
将 `force_cast` 和 `force_try` 从 `disabled_rules` 列表中移除。

### 变更
- 删除前: `disabled_rules: [..., force_cast, force_try]`
- 删除后: `disabled_rules: [trailing_whitespace, line_length, identifier_name, type_name, function_body_length, file_length, cyclomatic_complexity, nesting]`
- 增加注释: `# ASIL-B(D) 安全关键代码不允许 force_cast / force_try`

### 效果
SwiftLint 将默认启用 `force_cast` 和 `force_try` 规则，任何使用 `as!` 或 `try!` 的代码将产生编译错误（error 级别）。

## 验证
- ✅ force_cast / force_try 不在 disabled_rules 中
- ✅ 现有 Swift 代码无 force_cast / force_try 使用（grep 已验证）
