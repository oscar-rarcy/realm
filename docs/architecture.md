# Realm Architecture

Realm is split around explicit boundaries: input creates typed commands, command dispatch validates issuer intent, domain services mutate gameplay state, simulation advances time, AI plans commands, renderers consume render data, and platform code owns app side effects.

## Runtime flow

```text
platform/input -> Command -> dispatchCommand(GameContext&, Command)
               -> domain service / simulation helper
               -> GameEvent through EventSink
               -> renderer or UI status handling
```

`g` remains the app-level compatibility game object. New command, domain, AI, validation, save/load, and render-model code should accept explicit context/state where practical and keep direct `g` use at platform or legacy wrapper boundaries.

## Main modules

| Area | Responsibility |
| --- | --- |
| `src/commands/` | Typed command payloads, issuer-aware dispatch, context-command resolution, selection adapters. |
| `src/core/` | Core definitions, query/index helpers, validation/recovery, domain services. |
| `src/sim/` | Per-tick systems, save/load serialization, app-level simulation orchestration. |
| `src/ai/` | AI world view, tuning, planning, and planned command execution. |
| `src/map/` | Map-generation passes and invariants. |
| `src/render/` | Renderer-neutral model and ASCII/SDL presentation. |
| `src/platform/` | Main loops, app config, startup, and compatibility boundaries. |

## Allowed dependencies

Higher-level adapters may depend inward: platform and renderers may include commands/core/sim interfaces; commands may call core services; AI may create typed commands; sim may call core validation and query helpers.

## Forbidden dependencies

Renderers must not decide gameplay rules or mutate resources, production, orders, or combat. Input must not call low-level order/build/train/spawn helpers directly. AI planners must not call order/domain services directly; they plan commands and execute them through dispatch. Core services must not depend on platform or renderer modules.

## Validation and recovery policy

Validation reports stable issue codes, severity, message, optional entity ID, and optional tile. Recovery is explicit through `recoverGameState(...)`; callers should inspect issues first rather than relying on hidden repair during simulation.

Recoverable issues are limited to transient references that can be safely discarded or recalculated, such as stale selections, control-group IDs, target IDs, action markers, projectile coordinates, and path cache corruption. Path indices and out-of-bounds path points are recoverable because entity paths are derived navigation caches; recovery clears invalid path data and may return the unit to idle. Entity identity, type, owner, position, resource accounting, and production/research counter corruption are hard errors.
