#!/usr/bin/env bash
# yuleDKCS 集成测试 runner — 契约层 (pytest) + 组件层 (Go E2E, best-effort)
# 输出: tests/integration/test-output/ + .osh/ci/integration-results.json
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/tests/integration/test-output"
mkdir -p "$OUT"

echo "═══ [1/2] pytest 契约层集成测试 ═══"
cd "$ROOT"
python3 -m pytest tests/integration -q 2>&1 | tee "$OUT/pytest.log"
PY_EXIT=${PIPESTATUS[0]}

echo "═══ [2/2] Go 组件层集成测试 (best-effort, 失败不阻塞) ═══"
cd "$ROOT/backend/cloud/hub/tests/integration" || exit 1
if go test -tags=integration -count=1 -timeout 60s ./... 2>&1 | tee "$OUT/go-integration.log"; then
    GO_EXIT=0
else
    GO_EXIT=$?
    echo "⚠️  Go 集成套件未通过 (exit=$GO_EXIT) — 记录结果，不阻塞 CI 契约层"
fi

python3 - "$ROOT" "$PY_EXIT" "$GO_EXIT" <<'EOF'
import json, sys, datetime
root, py_exit, go_exit = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
report = {
    "layer": "integration",
    "generated_at": datetime.datetime.now().isoformat(),
    "pytest_exit": py_exit,
    "go_e2e_exit": go_exit,
    "status": "passed" if (py_exit == 0 and go_exit == 0) else "partial",
    "note": "pytest contract layer is the CI gate; Go E2E is best-effort",
}
with open(f"{root}/.osh/ci/integration-results.json", "w") as f:
    json.dump(report, f, indent=2)
print("📄 integration results ->", f"{root}/.osh/ci/integration-results.json")
EOF

echo "═══ done (pytest=$PY_EXIT, go=$GO_EXIT) ═══"
exit "$PY_EXIT"
