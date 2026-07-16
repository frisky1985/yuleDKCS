/**
 * @file port.c
 * @brief FreeRTOS Cortex-M7 Port — NXP S32K312
 *
 * ARMv7-M with FPU (fpv5-sp-d16), 16 priority levels, SysTick
 *
 * Implements:
 *   - vPortStartScheduler
 *   - vPortSVCHandler      (SVC_Handler)
 *   - xPortPendSVHandler   (PendSV_Handler)
 *   - xPortSysTickHandler  (SysTick_Handler)
 *   - vPortEnterCritical / vPortExitCritical
 *   - xPortIsInsideInterrupt
 *   - pxPortInitialiseStack
 */

/* FreeRTOS.h must come first — pulls in Std_Types.h (uint32, etc.) */
#include "FreeRTOS.h"
#include "portmacro.h"
#include "task.h"
#include "projdefs.h"

/* =========================================================================
 * Constants
 * ========================================================================= */
#define portINITIAL_XPSR                    0x01000000UL    /* Thumb bit set */
#define portINITIAL_CONTROL_IF_FPU          0x02            /* FPU active (FPCCR.ASPEN = CONTROL[2]) */
#define portINITIAL_CONTROL_REG             0x00            /* Privileged, no FPU */
#define portNO_CRITICAL_NESTING             0

/* NVIC registers */
#define portNVIC_INT_CTRL_REG               (*((volatile uint32*) 0xE000ED04))
#define portNVIC_PENDSVSET_BIT              (1UL << 28UL)
#define portNVIC_PENDSV_CLEAR_BIT           (1UL << 27UL)
#define portNVIC_SYSPRI2_REG                (*((volatile uint32*) 0xE000ED20))
#define portNVIC_SHPR3_REG                  (*((volatile uint32*) 0xE000ED24))
#define portNVIC_SYSTICK_CTRL_REG           (*((volatile uint32*) 0xE000E010))
#define portNVIC_SYSTICK_LOAD_REG           (*((volatile uint32*) 0xE000E014))
#define portNVIC_SYSTICK_CURRENT_VALUE_REG  (*((volatile uint32*) 0xE000E018))
#define portNVIC_SYSTICK_CLK_BIT            (1UL << 2UL)
#define portNVIC_SYSTICK_INT_BIT            (1UL << 1UL)
#define portNVIC_SYSTICK_ENABLE_BIT         (1UL << 0UL)
#define portNVIC_SYSTICK_COUNT_FLAG_BIT     (1UL << 16UL)

/* FPU registers */
#define portFPCCR                           (*((volatile uint32*) 0xE000EF34))
#define portFPCAR                           (*((volatile uint32*) 0xE000EF38))

/* =========================================================================
 * Kernel control block pointer
 * ========================================================================= */
extern void *pxCurrentTCB;
extern void vTaskSwitchContext(void);
extern void vTaskIncrementTick(void);

/* Forward declarations */
static void vPortSetupTimerInterrupt(void);

/* =========================================================================
 * Critical nesting counter
 * ========================================================================= */
static volatile uint32 uxCriticalNesting = 0x5A5A;      /* Magic uninit value */
static volatile BaseType_t xSchedulerRunning = pdFALSE;
static volatile uint32 uxInterruptNesting = 0;

/* =========================================================================
 * Stack Initialization
 * ========================================================================= */
StackType_t *pxPortInitialiseStack(
    StackType_t *pxTopOfStack,
    TaskFunction_t pxCode,
    void *pvParameters)
{
    /* Simulate a stack frame after an ISR saves context:
     * xPSR, ReturnAddress, LR (R14), R12, R3, R2, R1, R0
     * Then: R11, R10, R9, R8, R7, R6, R5, R4
     * If FPU enabled: S16..S31 (16 regs), FPSCR
     */

    /* Offset to make stack aligned to 8 bytes */
    pxTopOfStack--;

    /* Auto-saved registers (from hardware) */
    *pxTopOfStack = portINITIAL_XPSR;           /* xPSR — Thumb bit */
    pxTopOfStack--;
    *pxTopOfStack = ((StackType_t) pxCode);     /* PC — entry point */
    pxTopOfStack--;
    *pxTopOfStack = 0xFFFFFFFDUL;               /* LR — return to thread mode (main stack) */
    pxTopOfStack--;
    *pxTopOfStack = 0x12121212UL;               /* R12 */
    pxTopOfStack--;
    *pxTopOfStack = 0x03030303UL;               /* R3 */
    pxTopOfStack--;
    *pxTopOfStack = 0x02020202UL;               /* R2 */
    pxTopOfStack--;
    *pxTopOfStack = 0x01010101UL;               /* R1 */
    pxTopOfStack--;
    *pxTopOfStack = ((StackType_t) pvParameters);   /* R0 — task param */
    pxTopOfStack--;

    /* Callee-saved registers (manually saved) R4-R11 */
    *pxTopOfStack = 0x04040404UL;               /* R4 */
    pxTopOfStack--;
    *pxTopOfStack = 0x05050505UL;               /* R5 */
    pxTopOfStack--;
    *pxTopOfStack = 0x06060606UL;               /* R6 */
    pxTopOfStack--;
    *pxTopOfStack = 0x07070707UL;               /* R7 */
    pxTopOfStack--;
    *pxTopOfStack = 0x08080808UL;               /* R8 */
    pxTopOfStack--;
    *pxTopOfStack = 0x09090909UL;               /* R9 */
    pxTopOfStack--;
    *pxTopOfStack = 0x10101010UL;               /* R10 */
    pxTopOfStack--;
    *pxTopOfStack = 0x11111111UL;               /* R11 */
    pxTopOfStack--;

    /* FPU registers S16-S31 + FPSCR — if FPU enabled */
    #if (portFPU_ENABLED == 1)
    {
        uint32 i;
        for (i = 0; i < 16; i++) {
            *pxTopOfStack = 0x00000000UL;       /* S16..S31 = 0 */
            pxTopOfStack--;
        }
        *pxTopOfStack = 0x00000000UL;           /* FPSCR = 0 (default rounding, no exceptions) */
        pxTopOfStack--;
    }
    #endif

    /* Store a marker for the initial FPU state in the top of the EXC_RETURN */
    /* This will be used by xPortPendSVHandler */

    /* Return the adjusted stack pointer */
    return pxTopOfStack;
}

/* =========================================================================
 * Start Scheduler
 * ========================================================================= */
BaseType_t xPortStartScheduler(void)
{
    /* Configure PendSV and SysTick priorities */
    portNVIC_SYSPRI2_REG |= portNVIC_SVC_PRI;
    portNVIC_SHPR3_REG |= (portNVIC_PENDSV_PRI << 16UL);
    portNVIC_SHPR3_REG |= (portNVIC_SYSTICK_PRI_VAL << 24UL);

    /* Reset critical nesting counter */
    uxCriticalNesting = 0;

    /* Mark scheduler as running */
    xSchedulerRunning = pdTRUE;

    /* Initialize SysTick */
    vPortSetupTimerInterrupt();

    /* Enable FPU if configured */
    #if (portFPU_ENABLED == 1)
    {
        /* CPACR register: Enable CP10 and CP11 (full access) */
        volatile uint32 ulCPACR;
        ulCPACR = *((volatile uint32*) 0xE000ED88);
        ulCPACR |= (0xFUL << 20UL);
        *((volatile uint32*) 0xE000ED88) = ulCPACR;
    }
    #endif

    /* Trigger SVC to start first task */
    __asm volatile(
        "   cpsie   i                       \n"  /* Enable interrupts */
        "   dsb                             \n"
        "   isb                             \n"
        "   svc 0                           \n"  /* First task switch */
        "   nop                             \n"
        :
        :
        : "memory"
    );

    /* Should not reach here */
    return 0;
}

/* =========================================================================
 * End Scheduler
 * ========================================================================= */
void vPortEndScheduler(void)
{
    xSchedulerRunning = pdFALSE;

    /* Disable SysTick */
    portNVIC_SYSTICK_CTRL_REG = 0;

    /* Restore default priorities */
    uxCriticalNesting = portNO_CRITICAL_NESTING;
}

/* =========================================================================
 * Setup Timer (SysTick)
 * ========================================================================= */
void vPortSetupTimerInterrupt(void)
{
    /* Stop and clear timer */
    portNVIC_SYSTICK_CTRL_REG = 0;
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0;

    /* Set reload value for 1ms tick */
    portNVIC_SYSTICK_LOAD_REG = (configCPU_CLOCK_HZ / configTICK_RATE_HZ) - 1UL;

    /* Enable: SysTick, interrupt, use processor clock */
    portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT |
                                 portNVIC_SYSTICK_INT_BIT |
                                 portNVIC_SYSTICK_ENABLE_BIT;
}

/* =========================================================================
 * SVC Handler — Start first task
 * ========================================================================= */
void vPortSVCHandler(void)
{
    __asm volatile(
        "   ldr r3, =pxCurrentTCB            \n"  /* Get TCB pointer address */
        "   ldr r1, [r3]                     \n"  /* Get TCB value */
        "   ldr r0, [r1]                     \n"  /* Get first member (top of stack) */
        "   ldmia r0!, {r4-r11}              \n"  /* Pop R4-R11 */
        #if (portFPU_ENABLED == 1)
        "   vldmia r0!, {s16-s31}            \n"  /* Pop FPU regs S16-S31 */
        "   vldmia r0!, {s0-s15}             \n"  /* Pop FPU regs S0-S15 */
        #endif
        "   msr psp, r0                      \n"  /* Set PSP to remaining stack */
        "   mov r0, #0                       \n"
        "   msr basepri, r0                  \n"  /* Restore BASEPRI */
        "   orr r14, r14, #13                \n"  /* EXC_RETURN: return to thread, PSP, FPU active */
        "   bx r14                           \n"  /* Exception return */
        :
        :
        : "r0", "r1", "r3", "r14", "memory"
    );
}

/* =========================================================================
 * PendSV Handler — Context switch
 * ========================================================================= */
void xPortPendSVHandler(void)
{
    __asm volatile(
        "   mrs r0, psp                      \n"  /* Get PSP (current task stack) */
        "   isb                              \n"
        "   stmdb r0!, {r4-r11}              \n"  /* Save R4-R11 */
#if (portFPU_ENABLED == 1)
        "   tst r14, #0x10                   \n"  /* Test FPU active bit in EXC_RETURN */  
        "   it eq                            \n"
        "   vstmdbeq r0!, {s16-s31}          \n"  /* Save FPU regs if active (conditional store) */
#endif
        "   ldr r3, =pxCurrentTCB            \n"  /* Get TCB pointer address */
        "   ldr r2, [r3]                     \n"  /* Get current TCB */
        "   str r0, [r2]                     \n"  /* Save new top of stack to TCB->pxTopOfStack */
        "   stmdb sp!, {r3, r14}             \n"  /* Save R3 (TCB addr) and R14 (EXC_RETURN) */
        "   mov r0, #0x50                    \n"
        "   msr basepri, r0                  \n"  /* Mask interrupts */
        "   dsb                              \n"
        "   isb                              \n"
        "   bl vTaskSwitchContext            \n"  /* Select next task */
        "   mov r0, #0                       \n"
        "   msr basepri, r0                  \n"  /* Unmask interrupts */
        "   ldmia sp!, {r3, r14}             \n"  /* Restore R3, R14 */
        "   ldr r1, [r3]                     \n"  /* Get new TCB */
        "   ldr r0, [r1]                     \n"  /* Get new top of stack */
#if (portFPU_ENABLED == 1)
        "   tst r14, #0x10                   \n"  /* Check FPU active for new task */
        "   it eq                            \n"
        "   vldmiaeq r0!, {s16-s31}          \n"  /* Restore FPU regs if active */
#endif
        "   ldmia r0!, {r4-r11}              \n"  /* Pop R4-R11 */
        "   msr psp, r0                      \n"  /* Set PSP */
        "   isb                              \n"
        "   bx r14                           \n"  /* Exception return */
        :
        :
        : "r0", "r1", "r2", "r3", "r14", "memory"
    );
}

/* =========================================================================
 * SysTick Handler — Increment tick count
 * ========================================================================= */
void xPortSysTickHandler(void)
{
    /* Only process if scheduler is running */
    if (xSchedulerRunning != pdTRUE) {
        return;
    }

    /* Increment the tick count */
    vTaskIncrementTick();

    /* Check for context switch — tasks.c handles xYieldPending */
    /* PendSV will fire if a task yields */
}

/* =========================================================================
 * Enter Critical Section
 * ========================================================================= */
void vPortEnterCritical(void)
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;

    /* Must be called after scheduler has started */
    if (xSchedulerRunning == pdTRUE) {
        __asm volatile(" dsb \n isb \n" ::: "memory");
    }
}

/* =========================================================================
 * Exit Critical Section
 * ========================================================================= */
void vPortExitCritical(void)
{
    if (uxCriticalNesting > 0) {
        uxCriticalNesting--;
    }

    if (uxCriticalNesting == 0) {
        portENABLE_INTERRUPTS();
    }
}

/* =========================================================================
 * Check if inside interrupt
 * ========================================================================= */
BaseType_t xPortIsInsideInterrupt(void)
{
    BaseType_t xReturn;
    __asm volatile(
        "   mrs r0, ipsr                    \n"
        "   cmp r0, #0                      \n"
        "   it ne                           \n"
        "   movne %0, #1                    \n"
        "   it eq                           \n"
        "   moveq %0, #0                    \n"
        : "=r" (xReturn)
        :
        : "r0"
    );
    return xReturn;
}

/* =========================================================================
 * Set/clear interrupt mask
 * ========================================================================= */
UBaseType_t uxPortSetInterruptMask(void)
{
    uint32 ulReturn;
    __asm volatile(
        "   mrs r0, basepri                 \n"
        "   mov r1, #0x50                   \n"
        "   msr basepri, r1                 \n"
        "   dsb                             \n"
        "   isb                             \n"
        "   mov %0, r0                      \n"
        : "=r" (ulReturn)
        :
        : "r0", "r1"
    );
    return ulReturn;
}

void vPortClearInterruptMask(UBaseType_t ulMask)
{
    __asm volatile(
        "   msr basepri, %0                 \n"
        :
        : "r" (ulMask)
    );
}
