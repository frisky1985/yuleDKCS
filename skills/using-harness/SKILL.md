---
name: using-harness
description: Force Harness mode for task decomposition, routing, package-first execution, and verifier-first completion evidence.
---

# using-harness

## When to use
Use this skill whenever the task involves:
- project advancement
- dispatch
- execution
- verification
- delivery
- governance escalation

## Mandatory order
1. Lock Task Object first:
   - TaskType
   - BusinessObjective
   - TargetObject
   - MinimumSuccessArtifact
   - VerifierRole
   - BlockerCondition
2. Choose route from TASK_ROUTING_MATRIX.
3. Create Task Package before downstream execution.
4. Create Verifier Package before independent verification.
5. Do not claim VERIFIED or BusinessClosed without verifier evidence.

## Forbidden
- Do not jump into cron / shell / wrapper troubleshooting first.
- Do not dispatch without a formal package.
- Do not let verifier repair files.
- Do not narrate CREATED / DELIVERED into VERIFIED.