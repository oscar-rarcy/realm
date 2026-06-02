#pragma once

enum class InputIntent {
    None,
    Cancel,
    Confirm,
    CursorUp,
    CursorDown,
    CursorLeft,
    CursorRight,
    CursorFastUp,
    CursorFastDown,
    CursorFastLeft,
    CursorFastRight,
    ToggleHelp,
    TogglePause,
    Resign,
    Save,
    Load,
    Select,
    Context,
    BuildMenu,
    TrainMenu,
    RallyResearchTradeMenu,
    EjectGarrison,
    ToggleDiagnosticsOrTrebuchet,
    ToggleGate,
    RevealMapDebug,
    AttackMoveOrSelectArmy,
    HoldPosition,
    GroupAssign,
    ControlGroup,
    CycleIdleWorker,
    CycleUnit,
    HomeBase,
    Mouse
};

struct InputIntentResult {
    InputIntent intent = InputIntent::None;
    int slot = -1;
};

InputIntentResult inputIntentFromKey(int ch);
