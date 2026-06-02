# Testing

The primary regression command is `mingw32-make test` on Windows/MSYS2 or `make test` on Unix-like environments. The test binary is headless and uses deterministic `initGameWithSeed()` setup.

Regression tests should cover commands, services, save/load, validation/recovery, `WorldIndex` parity, map invariants, and deterministic AI smoke behavior without requiring SDL or terminal input.

## Allowed dependencies

Tests may create local deterministic game state, dispatch typed commands, inspect `CommandResult`, validate emitted events where available, and use fixture save paths under build/test output.

## Forbidden dependencies

Tests should not rely on renderer screenshots for core command/domain behavior. Avoid tests that depend on global UI text when a command result, service result, event, or direct state assertion is available.
