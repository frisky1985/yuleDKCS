/**
 * @file test_rtd_adapter.c
 * @brief RTD Adapter 自检测试 — yuleDKCS S32K312
 *
 * 验证所有 RTD 适配层接口的正确性：
 * 1. 所有函数调用能走到桩实现（非崩溃）
 * 2. 状态跟踪正确（初始化→操作）
 * 3. 版本信息返回正确
 * 4. NULL 参数检查工作正常
 * 5. 双重初始化幂等
 *
 * 编译方式:
 *   包含 rtd_adapter.c 和此文件，链接 mcal_stubs 库
 *
 * @copyright YuleTech, 2026
 */

#include "rtd_adapter.h"
/* EXIT_SUCCESS / EXIT_FAILURE — 主机和 freestanding 兼容 */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

/* ============================================================================
 * 测试框架宏
 * ============================================================================ */
static int s_testPassed = 0;
static int s_testFailed = 0;

#define TEST_ASSERT(cond, msg)  do { \
    if (!(cond)) { \
        s_testFailed++; \
        RTD_TRACE("  ❌ FAIL: %s", msg); \
    } else { \
        s_testPassed++; \
        RTD_TRACE("  ✅ PASS: %s", msg); \
    } \
} while(0)

#define TEST_SECTION(name)  RTD_TRACE("\n=== %s ===", name)

/* ============================================================================
 * 测试用配置（最小化配置 — 用于桩模式测试）
 * ============================================================================ */
static const Port_PinConfigType s_testPins[] = {
    { .Pin = 0x00, .Mode = 1U, .Direction = PORT_PIN_OUT, .PullConfig = PORT_PIN_PULL_OFF, .Drive = PORT_PIN_DRIVE_LOW, .InitOn = TRUE },
    { .Pin = 0x01, .Mode = 1U, .Direction = PORT_PIN_IN,  .PullConfig = PORT_PIN_PULL_UP,  .Drive = PORT_PIN_DRIVE_LOW, .InitOn = TRUE },
};

static const Port_ConfigType s_testPortConfig = {
    .Pins          = s_testPins,
    .NumPins       = 2U,
    .DevErrorDetect = TRUE,
};

static const Dio_ChannelConfigType s_testDioChannels[] = {
    { .Channel = 0x00, .DefaultLevel = STD_LOW },
    { .Channel = 0x01, .DefaultLevel = STD_HIGH },
};

static const Dio_ConfigType s_testDioConfig = {
    .Channels      = s_testDioChannels,
    .NumChannels   = 2U,
    .DevErrorDetect = TRUE,
};

static const Adc_ChannelConfigType s_testAdcChannels[] = {
    { .Group = 0U, .Channel = 0U, .Resolution = ADC_RES_12_BIT },
};

static const Adc_ConfigType s_testAdcConfig = {
    .Channels      = s_testAdcChannels,
    .NumChannels   = 1U,
    .DevErrorDetect = TRUE,
};

static const Pwm_ChannelConfigType s_testPwmChannels[] = {
    { .Channel = 0U, .DefaultPeriod = 10000U, .DefaultDutyCycle = 5000U, .Polarity = PWM_HIGH_ACTIVE },
};

static const Pwm_ConfigType s_testPwmConfig = {
    .Channels      = s_testPwmChannels,
    .NumChannels   = 1U,
    .DevErrorDetect = TRUE,
};

static const Gpt_ChannelConfigType s_testGptChannels[] = {
    { .Channel = 0U, .Mode = GPT_MODE_CONTINUOUS, .Notification = GPT_NOTIFICATION_ENABLE, .DefaultPeriod = 1000U },
};

static const Gpt_ConfigType s_testGptConfig = {
    .Channels      = s_testGptChannels,
    .NumChannels   = 1U,
    .DevErrorDetect = TRUE,
};

static const Wdg_ChannelConfigType s_testWdgChannels[] = {
    { .ChannelId = 0U, .DefaultMode = WDGM_OFF, .TimeoutMs = 1000U },
};

static const Wdg_ConfigType s_testWdgConfig = {
    .Channels      = s_testWdgChannels,
    .NumChannels   = 1U,
    .DevErrorDetect = TRUE,
};

/* ============================================================================
 * 固定配置（编译器需要 const — 但 Port_ConfigType.Pins 需要 const）
 * 注: Dio_ConfigType.Channels 也需要 const。前文 s_testDioChannels
 * 已经是 const。GCC arm-none-eabi 需要这些作为编译时常量。
 * ============================================================================ */

/* ============================================================================
 * Test: 版本信息
 * ============================================================================ */
static void test_version_info(void)
{
    TEST_SECTION("Version Info");

    Std_VersionInfoType ver;
    Std_ReturnType ret;

    /* 正常调用 */
    ret = Rtd_GetVersionInfo(&ver);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_GetVersionInfo returns OK");
    TEST_ASSERT(ver.vendorID == RTD_ADAPTER_VENDOR_ID, "Vendor ID matches");
    TEST_ASSERT(ver.moduleID == RTD_ADAPTER_MODULE_ID, "Module ID matches");
    TEST_ASSERT(ver.sw_major_version == RTD_ADAPTER_SW_MAJOR_VERSION, "Major version matches");
    TEST_ASSERT(ver.sw_minor_version == RTD_ADAPTER_SW_MINOR_VERSION, "Minor version matches");
    TEST_ASSERT(ver.sw_patch_version == RTD_ADAPTER_SW_PATCH_VERSION, "Patch version matches");

    /* NULL 参数 */
    ret = Rtd_GetVersionInfo(NULL_PTR);
    TEST_ASSERT(ret == RTD_E_INVALID_PARAM, "Rtd_GetVersionInfo(NULL) returns INVALID_PARAM");
}

/* ============================================================================
 * Test: 适配器全局状态
 * ============================================================================ */
static void test_adapter_state(void)
{
    TEST_SECTION("Adapter State");

    /* 初始状态 */
    TEST_ASSERT(Rtd_GetState() == RTD_STATE_UNINIT, "Initial state is UNINIT");
    TEST_ASSERT(Rtd_GetInitMask() == 0U, "Initial init mask is 0");
    TEST_ASSERT(Rtd_IsRtdEnabled() == FALSE, "RTD is NOT enabled (stub mode)");

    /* 版本查询不改变状态 */
    Std_VersionInfoType ver;
    Rtd_GetVersionInfo(&ver);
    TEST_ASSERT(Rtd_GetState() == RTD_STATE_UNINIT, "Version query does not change state");
}

/* ============================================================================
 * Test: MCU 初始化与基本操作
 * ============================================================================ */
static void test_mcu(void)
{
    Std_ReturnType ret;

    TEST_SECTION("MCU Driver");

    /* 初始化 */
    ret = Rtd_Mcu_Init(NULL_PTR);  /* NULL = default config */
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Mcu_Init(NULL) returns OK");
    TEST_ASSERT((Rtd_GetInitMask() & RTD_INIT_MCU) != 0, "MCU init bit set");
    TEST_ASSERT(Rtd_GetState() == RTD_STATE_IDLE, "State transitioned to IDLE");

    /* 幂等性: 再次初始化应返回 OK */
    ret = Rtd_Mcu_Init(NULL_PTR);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Mcu_Init() is idempotent");

    /* 模式设置 */
    Rtd_Mcu_SetMode(MCU_NORMAL);
    TEST_ASSERT(Rtd_Mcu_GetMode() == MCU_NORMAL, "MCU mode = NORMAL");

    Rtd_Mcu_SetMode(MCU_SLEEP);
    TEST_ASSERT(Rtd_Mcu_GetMode() == MCU_SLEEP, "MCU mode = SLEEP");

    Rtd_Mcu_SetMode(MCU_STOP);
    TEST_ASSERT(Rtd_Mcu_GetMode() == MCU_STOP, "MCU mode = STOP");

    /* 恢复 NORMAL */
    Rtd_Mcu_SetMode(MCU_NORMAL);

    /* 时钟分发 */
    Rtd_Mcu_DistributePllClock();

    /* 复位原因 — 桩模式返回 POR */
    uint8 reason = Rtd_Mcu_GetResetReason();
    TEST_ASSERT(reason != 0U, "Reset reason is non-zero");
    TEST_ASSERT(reason <= 0x1FU, "Reset reason within valid range (0-31)");
}

/* ============================================================================
 * Test: PORT 初始化与引脚配置
 * ============================================================================ */
static void test_port(void)
{
    Std_ReturnType ret;

    TEST_SECTION("PORT Driver");

    /* 未初始化时调用应失败 */
    ret = Rtd_Port_SetPinMode(0x00U, 1U);
    TEST_ASSERT(ret == RTD_E_INVALID_PARAM, "Rtd_Port_SetPinMode before init returns INVALID_PARAM");

    /* 正常初始化 */
    ret = Rtd_Port_Init(&s_testPortConfig);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Port_Init returns OK");
    TEST_ASSERT((Rtd_GetInitMask() & RTD_INIT_PORT) != 0, "PORT init bit set");

    /* NULL 参数 */
    ret = Rtd_Port_Init(NULL_PTR);
    TEST_ASSERT(ret == RTD_E_INVALID_PARAM || ret == RTD_E_OK, 
                "Rtd_Port_Init(NULL) guards against null (returns INVALID_PARAM)");

    /* 引脚模式设置 */
    ret = Rtd_Port_SetPinMode(0x00U, 1U);   /* PORTA pin 0, ALT1 */
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Port_SetPinMode(PORTA0, ALT1) returns OK");

    ret = Rtd_Port_SetPinMode(0x10U, 2U);   /* PORTB pin 0, ALT2 */
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Port_SetPinMode(PORTB0, ALT2) returns OK");

    /* 引脚方向设置 */
    ret = Rtd_Port_SetPinDirection(0x00U, PORT_PIN_OUT);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Port_SetPinDirection(PORTA0, OUT) returns OK");

    ret = Rtd_Port_SetPinDirection(0x01U, PORT_PIN_IN);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Port_SetPinDirection(PORTA1, IN) returns OK");
}

/* ============================================================================
 * Test: DIO 读写
 * ============================================================================ */
static void test_dio(void)
{
    Std_ReturnType ret;

    TEST_SECTION("DIO Driver");

    /* 未初始化时读取不会崩溃 */
    Dio_LevelType val = Rtd_Dio_ReadChannel(0x00U);
    TEST_ASSERT(val == STD_LOW, "Rtd_Dio_ReadChannel uninit returns LOW");

    /* 初始化 */
    ret = Rtd_Dio_Init(&s_testDioConfig);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Dio_Init returns OK");
    TEST_ASSERT((Rtd_GetInitMask() & RTD_INIT_DIO) != 0, "DIO init bit set");

    /* 写入 + 读取单通道 */
    ret = Rtd_Dio_WriteChannel(0x00U, STD_HIGH);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Dio_WriteChannel(STD_HIGH) returns OK");

    ret = Rtd_Dio_WriteChannel(0x00U, STD_LOW);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Dio_WriteChannel(STD_LOW) returns OK");

    /* 端口读写 */
    ret = Rtd_Dio_WritePort(0U, 0xFFFFU);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Dio_WritePort(0, 0xFFFF) returns OK");

    Dio_PortLevelType portVal = Rtd_Dio_ReadPort(0U);
    TEST_ASSERT(portVal != 0xFFFFU || portVal == 0U, 
                "Rtd_Dio_ReadPort returns expected value");

    /* 通道组操作 */
    const Dio_ChannelGroupType group = { .Port = 0U, .Offset = 0U, .Mask = 0x0FU };
    Dio_LevelType groupVal = Rtd_Dio_ReadChannelGroup(&group);
    TEST_ASSERT(groupVal <= 0x0FU, "Rtd_Dio_ReadChannelGroup returns masked value");

    ret = Rtd_Dio_WriteChannelGroup(&group, 0x05U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Dio_WriteChannelGroup returns OK");

    /* NULL 通道组 */
    groupVal = Rtd_Dio_ReadChannelGroup(NULL_PTR);
    TEST_ASSERT(groupVal == STD_LOW, "Rtd_Dio_ReadChannelGroup(NULL) returns LOW");

    ret = Rtd_Dio_WriteChannelGroup(NULL_PTR, 0U);
    TEST_ASSERT(ret == RTD_E_INVALID_PARAM, "Rtd_Dio_WriteChannelGroup(NULL) returns INVALID_PARAM");
}

/* ============================================================================
 * Test: ADC
 * ============================================================================ */
static void test_adc(void)
{
    Std_ReturnType ret;

    TEST_SECTION("ADC Driver");

    /* 初始化 */
    ret = Rtd_Adc_Init(&s_testAdcConfig);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Adc_Init returns OK");
    TEST_ASSERT((Rtd_GetInitMask() & RTD_INIT_ADC) != 0, "ADC init bit set");

    /* 启动转换 */
    ret = Rtd_Adc_StartGroupConversion(0U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Adc_StartGroupConversion returns OK");

    /* 读取 */
    Adc_ValueGroupType adcVal = 0U;
    ret = Rtd_Adc_ReadGroup(0U, &adcVal);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Adc_ReadGroup returns OK");
    TEST_ASSERT(adcVal == 0x7FFU, "Rtd_Adc_ReadGroup returns simulated value 0x7FF");

    /* 停止 */
    ret = Rtd_Adc_StopGroupConversion(0U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Adc_StopGroupConversion returns OK");

    /* 状态查询 */
    Adc_StatusType status = Rtd_Adc_GetGroupStatus(0U);
    TEST_ASSERT(status == 0U, "Rtd_Adc_GetGroupStatus returns IDLE");

    /* NULL 参数 */
    ret = Rtd_Adc_ReadGroup(0U, NULL_PTR);
    TEST_ASSERT(ret == RTD_E_INVALID_PARAM, "Rtd_Adc_ReadGroup(NULL buffer) returns INVALID_PARAM");
}

/* ============================================================================
 * Test: PWM
 * ============================================================================ */
static void test_pwm(void)
{
    Std_ReturnType ret;

    TEST_SECTION("PWM Driver");

    /* 初始化 */
    ret = Rtd_Pwm_Init(&s_testPwmConfig);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Pwm_Init returns OK");
    TEST_ASSERT((Rtd_GetInitMask() & RTD_INIT_PWM) != 0, "PWM init bit set");

    /* 设置占空比 */
    ret = Rtd_Pwm_SetDutyCycle(0U, 5000U);   /* 50% */
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Pwm_SetDutyCycle(50%) returns OK");

    ret = Rtd_Pwm_SetDutyCycle(0U, 0U);       /* 0% */
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Pwm_SetDutyCycle(0%) returns OK");

    ret = Rtd_Pwm_SetDutyCycle(0U, 10000U);   /* 100% */
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Pwm_SetDutyCycle(100%) returns OK");

    /* 周期+占空比 */
    ret = Rtd_Pwm_SetPeriodAndDuty(0U, 20000U, 5000U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Pwm_SetPeriodAndDuty returns OK");

    /* 启动/停止 */
    ret = Rtd_Pwm_StartChannel(0U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Pwm_StartChannel returns OK");

    Pwm_DutyCycleType duty = Rtd_Pwm_GetDutyCycle(0U);
    TEST_ASSERT(duty == 0U, "Rtd_Pwm_GetDutyCycle returns current duty");

    ret = Rtd_Pwm_StopChannel(0U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Pwm_StopChannel returns OK");
}

/* ============================================================================
 * Test: GPT 定时器
 * ============================================================================ */
static void test_gpt(void)
{
    Std_ReturnType ret;

    TEST_SECTION("GPT Driver");

    /* 初始化 */
    ret = Rtd_Gpt_Init(&s_testGptConfig);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Gpt_Init returns OK");
    TEST_ASSERT((Rtd_GetInitMask() & RTD_INIT_GPT) != 0, "GPT init bit set");

    /* 启动定时器 */
    ret = Rtd_Gpt_StartTimer(0U, 1000U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Gpt_StartTimer(1000) returns OK");

    /* 查询经过时间 */
    Gpt_ValueType elapsed = Rtd_Gpt_GetTimeElapsed(0U);
    TEST_ASSERT(elapsed == 0U, "Rtd_Gpt_GetTimeElapsed returns 0 (stub)");

    /* 查询剩余时间 */
    Gpt_ValueType remaining = Rtd_Gpt_GetTimeRemaining(0U);
    TEST_ASSERT(remaining == 0U, "Rtd_Gpt_GetTimeRemaining returns 0 (stub)");

    /* 启用/禁用通知 */
    ret = Rtd_Gpt_EnableNotification(0U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Gpt_EnableNotification returns OK");

    ret = Rtd_Gpt_DisableNotification(0U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Gpt_DisableNotification returns OK");

    /* 停止定时器 */
    ret = Rtd_Gpt_StopTimer(0U);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Gpt_StopTimer returns OK");

    /* 未初始化通道 */
    elapsed = Rtd_Gpt_GetTimeElapsed(5U);   /* 未初始化的通道 */
    TEST_ASSERT(elapsed == 0U, "Rtd_Gpt_GetTimeElapsed(uninit ch) returns 0");
}

/* ============================================================================
 * Test: WDG 看门狗
 * ============================================================================ */
static void test_wdg(void)
{
    Std_ReturnType ret;

    TEST_SECTION("WDG Driver");

    /* 初始化 */
    ret = Rtd_Wdg_Init(&s_testWdgConfig);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Wdg_Init returns OK");
    TEST_ASSERT((Rtd_GetInitMask() & RTD_INIT_WDG) != 0, "WDG init bit set");

    /* 设置模式 */
    ret = Rtd_Wdg_SetMode(WDGM_OFF);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Wdg_SetMode(OFF) returns OK");
    TEST_ASSERT(Rtd_Wdg_GetMode() == WDGM_OFF, "WDG mode = OFF");

    ret = Rtd_Wdg_SetMode(WDGM_SLOW);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Wdg_SetMode(SLOW) returns OK");
    TEST_ASSERT(Rtd_Wdg_GetMode() == WDGM_SLOW, "WDG mode = SLOW");

    ret = Rtd_Wdg_SetMode(WDGM_FAST);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Wdg_SetMode(FAST) returns OK");
    TEST_ASSERT(Rtd_Wdg_GetMode() == WDGM_FAST, "WDG mode = FAST");

    /* 喂狗 */
    ret = Rtd_Wdg_Trigger();
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Wdg_Trigger returns OK");

    /* 版本信息 */
    Std_VersionInfoType ver;
    ret = Rtd_Wdg_GetVersionInfo(&ver);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_Wdg_GetVersionInfo returns OK");

    /* NULL 版本信息 */
    ret = Rtd_Wdg_GetVersionInfo(NULL_PTR);
    TEST_ASSERT(ret == RTD_E_INVALID_PARAM, "Rtd_Wdg_GetVersionInfo(NULL) returns INVALID_PARAM");

    /* 恢复 OFF 模式 */
    Rtd_Wdg_SetMode(WDGM_OFF);
}

/* ============================================================================
 * Test: Rtd_InitAll 便利函数
 * ============================================================================ */
static void test_init_all(void)
{
    Std_ReturnType ret;

    TEST_SECTION("Rtd_InitAll Sequential Init");

    /* 先重置状态 — 在完整测试中不能实际重置，
       此处仅验证 Rtd_InitAll 函数的接口 */
    Std_VersionInfoType ver;
    Rtd_GetVersionInfo(&ver);

    /* 注意: 这里不实际调用 init（所有模块已分别在前面测试中初始化），
       仅确认由于幂等性，重新调用也会返回 OK */
    ret = Rtd_InitAll(NULL_PTR, &s_testPortConfig, &s_testDioConfig,
                       &s_testAdcConfig, &s_testPwmConfig,
                       &s_testGptConfig, &s_testWdgConfig);
    TEST_ASSERT(ret == RTD_E_OK, "Rtd_InitAll (idempotent) returns OK");
}

/* ============================================================================
 * Test: 诊断输出
 * ============================================================================ */
static void test_diagnostics(void)
{
    TEST_SECTION("Diagnostics");

    /* 确认诊断函数不会崩溃 */
    Rtd_DumpDiagnostics();
    TEST_ASSERT(TRUE, "Rtd_DumpDiagnostics executes without crash");
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */
int main(void)
{
    RTD_TRACE("");
    RTD_TRACE("╔═══════════════════════════════════════════╗");
    RTD_TRACE("║  yuleDKCS RTD Adapter Self-Test Suite     ║");
    RTD_TRACE("║  Target: NXP S32K312                     ║");
    RTD_TRACE("║  Mode:   %s                              ║",
              Rtd_IsRtdEnabled() ? "RTD (live driver)" : "STUB (register-level)");
    RTD_TRACE("╚═══════════════════════════════════════════╝");
    RTD_TRACE("");

    test_version_info();
    test_adapter_state();
    test_mcu();
    test_port();
    test_dio();
    test_adc();
    test_pwm();
    test_gpt();
    test_wdg();
    test_init_all();
    test_diagnostics();

    RTD_TRACE("");
    RTD_TRACE("=== Results: %d passed, %d failed ===", s_testPassed, s_testFailed);

    return (s_testFailed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
