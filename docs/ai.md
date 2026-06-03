# AI

AI builds an `AIWorldView`, reads centralized `AITuning`, appends typed commands into `AIContext::plannedCommands`, then calls `executeAICommands()` to validate those commands through the normal dispatcher.

`AIIntel` stores stable player-base ID/position data instead of raw entity pointers across planning and mutation boundaries.

## Allowed dependencies

AI may read `Game`, `WorldIndex`, core definitions, and query helpers. AI may construct typed `Command` payloads and inspect `CommandResult` rejections for diagnostics. Tuning constants belong in `AITuning`.

## Forbidden dependencies

AI planners must not call low-level `order*`, domain `start*`, or `ejectGarrison` helpers directly. AI should not directly mutate order flags such as `attackMove`; express intent as commands. AI should not cache raw `Entity*` pointers across command execution.
