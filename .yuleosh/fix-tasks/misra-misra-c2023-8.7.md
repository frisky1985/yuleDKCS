# MISRA Fix Task: misra-c2023-8.7

> Generated: 2026-08-05T21:57:21.437441
> Severity: advisory
> Spec Ref: SWE-MISRA-S1

## Rule: Functions and objects should not be defined with external linkage

除非必要，函数和对象不应使用外部链接定义（优先用 static）

## Violations

| # | File | Line | Col | Message |
|--:|:-----|:----|:----|:--------|
| 1 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/memif_impl.c` | 39 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 2 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/memif_impl.c` | 73 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 3 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/memif_impl.c` | 93 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 4 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/memif_impl.c` | 111 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 5 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/memif_impl.c` | 123 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 6 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/memif_impl.c` | 132 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 7 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/trng_stub.c` | 136 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 8 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/trng_stub.c` | 223 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 9 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/trng_stub.c` | 309 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 10 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/mcal_stubs.c` | 104 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 11 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/mcal_stubs.c` | 312 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 12 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/mcal_stubs.c` | 361 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 13 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/mcal_stubs.c` | 418 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 14 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/mcal_stubs.c` | 472 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 15 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/mcal_stubs.c` | 503 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 16 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/mcal_stubs/src/mcal_stubs.c` | 508 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 17 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/freertos_port/src/port.c` | 69 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 18 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/freertos_port/src/port.c` | 305 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 19 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/freertos_port/src/port.c` | 319 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 20 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/freertos_port/src/port.c` | 333 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 21 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/freertos_port/src/port.c` | 353 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 22 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/bsw_integration/mcal/rtd_adapter.c` | 709 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 23 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/bsw_integration/src/freertos_stubs.c` | 15 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 24 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/bsw_integration/src/freertos_stubs.c` | 38 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |
| 25 | `/Users/stefan/.openclaw/workspace/yuleDKCS/embedded/bsw_integration/src/freertos_stubs.c` | 39 | 0 | misra violation (use --rule-texts=<file> to get proper output) [misra-c2012-8.7] |

## Fix Checklist

- [ ] Understand the violation context
- [ ] Apply fix to source code
- [ ] Re-run MISRA check to verify fix
- [ ] Update traceability matrix
- [ ] Document deviation if fix is not feasible

---
*Generated by yuleOSH MISRA fix-task generator*