#include "commands/input_intent.h"
#include "input_keys.h"

const CommandHelpBinding* gameplayHelpBindings(int& count) {
    static const CommandHelpBinding commands[] = {
        {"select", "Select", "Space/click", "Normal", "Select unit or building", InputIntent::Select, -1, 1, {' '}},
        {"command", "Command", "Enter/R-click", "Normal", "Move, attack, gather, help, or garrison", InputIntent::Confirm, -1, 3, {'\n', '\r', KEY_ENTER}},
        {"help", "Help", "?", "Any", "Toggle in-game help", InputIntent::ToggleHelp, -1, 1, {'?'}},
        {"build", "Build", "B", "Normal", "Open peasant build menu", InputIntent::BuildMenu, -1, 2, {'b', 'B'}},
        {"train", "Train", "T", "Normal", "Open production queue; repeat unit keys to queue", InputIntent::TrainMenu, -1, 2, {'t', 'T'}},
        {"attack_move", "Attack move", "A", "Normal", "Select all military or set attack-move target", InputIntent::AttackMoveOrSelectArmy, -1, 1, {'A'}},
        {"rally", "Rally/research/trade", "R", "Normal", "Set rally point or open building utility menus", InputIntent::RallyResearchTradeMenu, -1, 2, {'r', 'R'}},
        {"groups", "Groups", "G, 1-9", "Normal", "Assign and recall control groups", InputIntent::GroupAssign, -1, 1, {'G'}},
        {"hold", "Hold position", "X", "Normal", "Stop and hold selected units in place", InputIntent::HoldPosition, -1, 2, {'x', 'X'}},
        {"pause", "Pause", "P", "Normal", "Toggle pause", InputIntent::TogglePause, -1, 2, {'p', 'P'}},
        {"diagnostics", "Diagnostics", "D", "Normal", "Toggle debug diagnostics", InputIntent::ToggleDiagnosticsOrTrebuchet, -1, 2, {'d', 'D'}},
        {"save", "Save", "V/F5-F8", "Normal", "Save current match slots", InputIntent::Save, 0, 2, {'v', 'V'}, KEY_F(5), KEY_F(8), 1 - KEY_F(5)},
        {"load", "Load", "L/F9-F12", "Normal", "Load saved match slots", InputIntent::Load, 0, 2, {'l', 'L'}, KEY_F(9), KEY_F(12), 1 - KEY_F(9)},
        {"resign", "Resign", "Q", "Normal/game over", "Return to main menu", InputIntent::Resign, -1, 2, {'q', 'Q'}},
        {"cancel", "Cancel", "Esc", "Command modes", "Cancel build, train, wall, rally, attack-move, or research", InputIntent::Cancel, -1, 1, {27}},
        {"zoom", "Zoom", "+/-/wheel", "SDL", "Zoom map", InputIntent::None},
        {"pan", "Pan", "Middle-drag", "SDL", "Pan map", InputIntent::None}
    };
    count = (int)(sizeof(commands) / sizeof(commands[0]));
    return commands;
}

static bool bindingMatches(const CommandHelpBinding& binding, int ch, int& slot) {
    for (int i = 0; i < binding.keyCount; i++) {
        if (binding.keyCodes[i] == ch) {
            slot = binding.slot;
            return binding.intent != InputIntent::None;
        }
    }
    if (binding.rangeStart != 0 && ch >= binding.rangeStart && ch <= binding.rangeEnd) {
        slot = ch + binding.rangeSlotOffset;
        return binding.intent != InputIntent::None;
    }
    return false;
}

InputIntentResult inputIntentFromKey(int ch) {
    switch (ch) {
    case ERR:       return { InputIntent::None, -1 };
    case KEY_UP:    return { InputIntent::CursorUp, -1 };
    case KEY_DOWN:  return { InputIntent::CursorDown, -1 };
    case KEY_LEFT:  return { InputIntent::CursorLeft, -1 };
    case KEY_RIGHT: return { InputIntent::CursorRight, -1 };
    case KEY_SR:
    case KEY_PPAGE: return { InputIntent::CursorFastUp, -1 };
    case KEY_SF:
    case KEY_NPAGE: return { InputIntent::CursorFastDown, -1 };
    case KEY_SLEFT:
    case KEY_HOME:  return { InputIntent::CursorFastLeft, -1 };
    case KEY_SRIGHT:
    case KEY_END:   return { InputIntent::CursorFastRight, -1 };
    case KEY_MOUSE: return { InputIntent::Mouse, -1 };
    case 'O':       return { InputIntent::ToggleGate, -1 };
    case 'S':       return { InputIntent::RevealMapDebug, -1 };
    case '.': case ',': return { InputIntent::CycleIdleWorker, -1 };
    case '\t':      return { InputIntent::CycleUnit, -1 };
    case 'h':       return { InputIntent::HomeBase, -1 };
    }
    if (ch >= '1' && ch <= '9') return { InputIntent::ControlGroup, ch - '1' };
    int n = 0;
    const CommandHelpBinding* bindings = gameplayHelpBindings(n);
    for (int i = 0; i < n; i++) {
        int slot = -1;
        if (bindingMatches(bindings[i], ch, slot)) return { bindings[i].intent, slot };
    }
    return { InputIntent::None, -1 };
}

std::string commandHelpLine() {
    int n = 0;
    const CommandHelpBinding* c = gameplayHelpBindings(n);
    std::string out;
    for (int i = 0; i < n; i++) {
        if (i) out += "  ";
        out += c[i].keys;
        out += ':';
        out += c[i].label;
    }
    return out;
}
