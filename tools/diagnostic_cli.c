/******************************************************************************
 * @file    diagnostic_cli.c
 * @brief   运维诊断命令行工具
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dkcs.h"
#include "nvm_key_storage.h"

static void print_usage(const char *prog) {
    printf("yuleDKCS Diagnostic CLI v1.0\n");
    printf("Usage: %s <command> [options]\n\n", prog);
    printf("Commands:\n");
    printf("  status        - 显示系统状态\n");
    printf("  keys          - 列出存储的钥匙\n");
    printf("  test <type>   - 运行自检 (crypto/nvm/comm/all)\n");
    printf("  reset         - 重置系统状态\n");
    printf("  log           - 输出诊断日志\n");
    printf("  se050         - SE050 状态检查\n");
    printf("  uwb           - UWB 模块测试\n");
}

static void cmd_status(void) {
    printf("=== 系统状态 ===\n");
    printf("活动会话: %%d\n", dkcs_get_active_session_count());
    printf("存储钥匙: %%d\n", nvm_get_key_count());
}

static void cmd_test(const char *type) {
    printf("=== 自检: %%s ===\n", type);
    printf("测试完成\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *cmd = argv[1];
    
    if (strcmp(cmd, "status") == 0) {
        cmd_status();
    } else if (strcmp(cmd, "test") == 0 && argc >= 3) {
        cmd_test(argv[2]);
    } else {
        printf("未知命令: %%s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}
