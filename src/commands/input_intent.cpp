#include "commands/input_intent.h"
#include "input_keys.h"

InputIntentResult inputIntentFromKey(int ch) {
    switch (ch) {
    case ERR:       return { InputIntent::None, -1 };
    case 27:        return { InputIntent::Cancel, -1 };
    case '\n':
    case '\r':
    case KEY_ENTER: return { InputIntent::Confirm, -1 };
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
    case '?':       return { InputIntent::ToggleHelp, -1 };
    case 'q': case 'Q': return { InputIntent::Resign, -1 };
    case 'p': case 'P': return { InputIntent::TogglePause, -1 };
    case 'v': case 'V': return { InputIntent::Save, 0 };
    case 'l': case 'L': return { InputIntent::Load, 0 };
    case ' ':       return { InputIntent::Select, -1 };
    case 'b': case 'B': return { InputIntent::BuildMenu, -1 };
    case 't': case 'T': return { InputIntent::TrainMenu, -1 };
    case 'r': case 'R': return { InputIntent::RallyResearchTradeMenu, -1 };
    case 'u': case 'U': return { InputIntent::EjectGarrison, -1 };
    case 'd': case 'D': return { InputIntent::ToggleDiagnosticsOrTrebuchet, -1 };
    case 'O':       return { InputIntent::ToggleGate, -1 };
    case 'S':       return { InputIntent::RevealMapDebug, -1 };
    case 'A':       return { InputIntent::AttackMoveOrSelectArmy, -1 };
    case 'x': case 'X': return { InputIntent::HoldPosition, -1 };
    case 'G':       return { InputIntent::GroupAssign, -1 };
    case '.': case ',': return { InputIntent::CycleIdleWorker, -1 };
    case '\t':      return { InputIntent::CycleUnit, -1 };
    case 'h':       return { InputIntent::HomeBase, -1 };
    }
    if (ch >= '1' && ch <= '9') return { InputIntent::ControlGroup, ch - '1' };
    if (ch >= KEY_F(5) && ch <= KEY_F(8)) return { InputIntent::Save, ch - KEY_F(5) + 1 };
    if (ch >= KEY_F(9) && ch <= KEY_F(12)) return { InputIntent::Load, ch - KEY_F(9) + 1 };
    return { InputIntent::None, -1 };
}
