#!/bin/bash
# batch-create.sh - 批量创建员工

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CREATE_SCRIPT="$SCRIPT_DIR/create-employee.sh"

# 颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

show_help() {
    cat << HELP
批量创建员工工具

用法:
  $0 <csv-file>

CSV 格式:
  姓名，工号，电话，描述 [，初始用户]

示例:
  姓名，工号，电话，描述
  客服工程师，CS-001,13800138000，客服团队
  销售顾问，SA-001,13600136000，销售团队
  运营专员，OP-001,13500135000，运营团队

HELP
}

if [ -z "$1" ]; then
    echo "❌ 用法：$0 <csv-file>"
    echo ""
    show_help
    exit 1
fi

CSV_FILE="$1"

if [ ! -f "$CSV_FILE" ]; then
    echo "❌ CSV 文件不存在：$CSV_FILE"
    exit 1
fi

if [ ! -f "$CREATE_SCRIPT" ]; then
    echo "❌ 创建脚本不存在：$CREATE_SCRIPT"
    exit 1
fi

echo "🚀 开始批量创建员工..."
echo "   CSV 文件：$CSV_FILE"
echo "========================================"
echo ""

# 统计
total=0
success=0
failed=0

# 读取 CSV 文件（跳过表头）
line_num=0
while IFS=, read -r name role phone description initial_user; do
    line_num=$((line_num + 1))
    
    # 跳过表头
    if [ $line_num -eq 1 ] && [[ "$name" == "姓名" || "$name" == "name" ]]; then
        continue
    fi
    
    # 去除空白
    name=$(echo "$name" | xargs)
    role=$(echo "$role" | xargs)
    phone=$(echo "$phone" | xargs)
    description=$(echo "$description" | xargs)
    initial_user=$(echo "$initial_user" | xargs)
    
    # 跳过空行
    if [ -z "$name" ] || [ -z "$role" ] || [ -z "$phone" ]; then
        echo "⚠️ 跳过空行 (行 $line_num)"
        continue
    fi
    
    total=$((total + 1))
    
    echo "📋 [$total] 创建：$name ($role)"
    
    if [ -n "$initial_user" ]; then
        bash "$CREATE_SCRIPT" "$name" "$role" "$phone" "$description" "$initial_user"
    else
        bash "$CREATE_SCRIPT" "$name" "$role" "$phone" "$description"
    fi
    
    if [ $? -eq 0 ]; then
        echo -e "   ${GREEN}✅ 成功${NC}"
        success=$((success + 1))
    else
        echo -e "   ${RED}❌ 失败${NC}"
        failed=$((failed + 1))
    fi
    
    echo ""
done < "$CSV_FILE"

# 总结
echo "========================================"
echo "📊 批量创建完成！"
echo ""
echo "   总计：$total"
echo -e "   ${GREEN}成功：$success${NC}"
echo -e "   ${RED}失败：$failed${NC}"
echo ""

if [ $failed -gt 0 ]; then
    echo "⚠️ 有 $failed 个员工创建失败，请检查日志"
    exit 1
fi
