# Commands

Commands are the frontend-neutral gameplay boundary. `Command` carries a `PlayerId issuer` and a typed `std::variant` payload such as `MoveCommand`, `BuildCommand`, `ResearchCommand`, `SaveCommand`, or `ToggleGateCommand`.

`dispatchCommand(GameContext&, const Command&)` is exhaustive over payload types and returns `CommandResult` with `Accepted`, `Rejected`, `NoOp`, or `Error`.

## Allowed dependencies

Command dispatch may use `GameContext`, `WorldIndex`, core domain services, save/load services, and event sinks. Input and AI may construct commands. Tests should prefer commands over direct entity mutation for gameplay actions.

## Forbidden dependencies

Handlers must not infer payload meaning from unrelated fields. New command payloads must not pack coordinates into integers. Dispatch must not assume owner `0`; use `command.issuer`. Input code must not bypass commands by calling `orderBuild`, `orderTrain`, `orderAttack`, `spawnEntity`, or resource mutation directly.
