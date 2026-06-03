#pragma once

#include <string>

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

struct CommandHelpBinding {
    const char* id;
    const char* label;
    const char* keys;
    const char* modes;
    const char* help;
    InputIntent intent = InputIntent::None;
    int slot = -1;
    int keyCount = 0;
    int keyCodes[4] = { 0, 0, 0, 0 };
    int rangeStart = 0;
    int rangeEnd = 0;
    int rangeSlotOffset = 0;
};

struct InputIntentResult {
    InputIntent intent = InputIntent::None;
    int slot = -1;
};

InputIntentResult inputIntentFromKey(int ch);
const CommandHelpBinding* gameplayHelpBindings(int& count);
std::string commandHelpLine();
