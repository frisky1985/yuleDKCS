/******************************************************************************
 * @file    yuleasr_bsw.h
 * @brief   yuleASR AutoSAR BSW Platform Integration Header
 * @author  YuleTech
 * @version 1.0.0
 * @date    2026-05-26
 *
 * yuleDKCS 应用层通过此头文件调用 yuleASR 平台的基础软件服务。
 *
 * 包含的 BSW 模块:
 *   - HAL/MCAL: 硬件抽象层, GPIO, SPI, UART, I2C
 *   - OS: 操作系统抽象 (任务调度, 中断, 时间管理)
 *   - COM: 通信栈 (CAN, LIN, Ethernet)
 *   - MEM: 存储服务 (NVM, EEPROM 抽象)
 *   - CRYPTO: 硬件密码加速 (若 MCU 支持)
 *
 * 条件编译:
 *   当 CMake 中 DETECT_YULEASR 为真时, YULEASR_PLATFORM 宏被定义。
 *   应用代码可通过 #ifdef YULEASR_PLATFORM 来启用/禁用平台相关代码。
 ******************************************************************************/

#ifndef YULEASR_BSW_H
#define YULEASR_BSW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * 平台检测
 * ============================================================================
 * 若通过 CMake 集成了 yuleASR, 编译时会自动定义 YULEASR_PLATFORM 宏。
 * 此处提供备用检测机制, 确保编译器的兼容性。
 */
#if defined(YULEASR_PLATFORM) || defined(USE_YULEASR)
    #define HAVE_YULEASR_BSW 1
#else
    #define HAVE_YULEASR_BSW 0
#endif

/* ============================================================================
 * BSW 模块头文件引用
 * ============================================================================
 * 以下头文件由 yuleASR 平台提供。若 HAVE_YULEASR_BSW 为 0,
 * 则使用 yuleDKCS 自带的 HAL 存根 (stub) 实现。
 */
#if HAVE_YULEASR_BSW
    /* ---- HAL/MCAL 抽象层 ---- */
    #include <yuleASR/hal/gpio.h>       /* GPIO 控制          */
    #include <yuleASR/hal/spi.h>        /* SPI 总线           */
    #include <yuleASR/hal/uart.h>       /* UART 串口          */
    #include <yuleASR/hal/timer.h>      /* 定时器             */
    #include <yuleASR/hal/interrupt.h>  /* 中断管理           */

    /* ---- OS 调度 ---- */
    #include <yuleASR/os/task.h>        /* 任务管理           */
    #include <yuleASR/os/semaphore.h>   /* 信号量             */
    #include <yuleASR/os/queue.h>       /* 消息队列           */

    /* ---- 通信栈 ---- */
    #include <yuleASR/com/can.h>        /* CAN 通信           */
    #include <yuleASR/com/lin.h>        /* LIN 通信           */

    /* ---- 存储 ---- */
    #include <yuleASR/mem/nvm.h>        /* 非易失性存储       */

    /* ---- 诊断 ---- */
    #include <yuleASR/diag/dcm.h>       /* 诊断通信管理       */
    #include <yuleASR/diag/dem.h>       /* 诊断事件管理       */
#endif /* HAVE_YULEASR_BSW */

/* ============================================================================
 * yuleASR BSW 版本信息
 * ============================================================================
 * 由 CMake 传递, 或在此处定义默认值。
 */
#ifndef YULEASR_VERSION_MAJOR
#define YULEASR_VERSION_MAJOR 1
#endif
#ifndef YULEASR_VERSION_MINOR
#define YULEASR_VERSION_MINOR 0
#endif
#ifndef YULEASR_VERSION_PATCH
#define YULEASR_VERSION_PATCH 0
#endif

#define YULEASR_VERSION_STRING \
    "yuleASR v" STRINGIFY(YULEASR_VERSION_MAJOR) "." \
    STRINGIFY(YULEASR_VERSION_MINOR) "." \
    STRINGIFY(YULEASR_VERSION_PATCH)

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif

/* ============================================================================
 * BSW 初始化函数
 * ============================================================================
 * yuleDKCS 启动时会调用此函数来完成 BSW 平台的初始化。
 * 若 HAVE_YULEASR_BSW 为 0, 则使用空实现。
 */

/**
 * @brief 初始化 yuleASR BSW 平台
 * @return 0 成功, 非零失败
 *
 * 此函数应:
 *   1. 初始化 MCAL (MCU 时钟, GPIO, 中断控制器)
 *   2. 初始化 OS (启动调度器)
 *   3. 初始化通信栈 (CAN/LIN)
 *   4. 初始化存储服务 (NVM)
 *   5. 初始化诊断模块
 */
int yuleasr_bsw_init(void);

/**
 * @brief 获取 BSW 版本字符串
 * @return 指向版本字符串的指针
 */
const char* yuleasr_bsw_version(void);

/* ============================================================================
 * 平台抽象接口 (当无 yuleASR 时的后备实现)
 * ============================================================================
 * 以下类型定义确保了在没有 yuleASR 时仍能编译通过。
 */

#if !HAVE_YULEASR_BSW
/* 简化的 HAL 类型 (实际使用时应由 yuleASR 提供) */
typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET   = 1
} gpio_pin_state_t;

/* 简化的 OS 类型 */
typedef uint32_t tick_t;
typedef void (*task_func_t)(void* param);

/* 简易延时宏 */
#define YULEASR_DELAY_MS(ms) /* 空实现, 生产环境需替换 */
#endif /* !HAVE_YULEASR_BSW */

#ifdef __cplusplus
}
#endif

#endif /* YULEASR_BSW_H */
