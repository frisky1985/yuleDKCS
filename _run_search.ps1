$params = @{
    keyword = "DeepSeek distillation arXiv 2026"
    cnt = 10
}
$json = $params | ConvertTo-Json -Compress
node "C:\Program Files\QClaw\resources\openclaw\config\skills\online-search\scripts\prosearch.cjs" $json
