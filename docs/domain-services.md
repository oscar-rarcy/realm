# Domain Services

Domain services own gameplay mutation that is not pure per-tick simulation. Existing services cover build, production, research, market, and transitional order actions for movement, combat, gather, garrison, rally, gate, and trebuchet stance.

Services validate issuer ownership and return structured success or rejection information for command dispatch to map into `CommandResult`.

## Allowed dependencies

Services may depend on core game types, definitions, query helpers, validation helpers, and temporary legacy order/simulation helpers while migration continues. They may emit `GameEvent` status through the current event compatibility layer.

## Forbidden dependencies

Services must not depend on renderer, platform, or input modules. They should not silently ignore invalid input; return a rejection reason. New services should not call `setStatus` or `addActionMarker` directly.
