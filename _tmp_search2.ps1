$from = [int64]([DateTimeOffset]::Now.AddDays(-7).ToUnixTimeSeconds())
$to = [int64]([DateTimeOffset]::Now.ToUnixTimeSeconds())
node "C:\Program Files\QClaw\resources\openclaw\config\skills\online-search\scripts\prosearch.cjs" "--json" "{\`"keyword\`":\`"AI大模型 行业报告 2026\`",\`"from_time\`":$from,\`"to_time\`":$to}"
