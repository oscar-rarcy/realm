#include "realm.h"
#include "view_state.h"
#include "input_keys.h"

#include "commands/command.h"
#include "commands/input_feedback.h"
#include "commands/input_intent.h"
#include "commands/input_mode_controller.h"
#include "core/market_service.h"

#include <cctype>
#include <climits>
#include <optional>

struct BuildMenuOption {
    char key;
    EntityType type;
};

struct TrainMenuOption {
    EntityType producer;
    char key;
    EntityType unit;
};

struct ResearchMenuOption {
    char key;
    ResearchId research;
};

static bool keyMatches(int ch, char key) {
    return ch >= 0 && ch <= UCHAR_MAX && std::tolower((unsigned char)ch) == key;
}

static EntityType buildMenuSelection(int ch) {
    static constexpr BuildMenuOption options[] = {
        {'h', E_HOUSE},
        {'b', E_BARRACKS},
        {'s', E_STABLE},
        {'t', E_TOWER},
        {'f', E_FARM},
        {'w', E_WALL},
        {'a', E_BLACKSMITH},
        {'c', E_CHURCH},
        {'m', E_MARKET},
        {'k', E_CASTLE},
        {'l', E_LUMBER_CAMP},
        {'n', E_MINING_CAMP},
        {'i', E_MILL},
        {'g', E_GATE},
        {'d', E_DOCK},
    };
    for (const BuildMenuOption& option : options)
        if (keyMatches(ch, option.key)) return option.type;
    return E_NONE;
}

static EntityType trainMenuSelection(EntityType producer, int ch) {
    static constexpr TrainMenuOption options[] = {
        {E_TOWNHALL, 'p', E_PEASANT},
        {E_BARRACKS, 'm', E_MILITIA},
        {E_BARRACKS, 'a', E_ARCHER},
        {E_BARRACKS, 's', E_SPEARMAN},
        {E_BARRACKS, 'c', E_CATAPULT},
        {E_BARRACKS, 'r', E_RAM},
        {E_STABLE, 'k', E_KNIGHT},
        {E_CASTLE, 'p', E_PEASANT},
        {E_CASTLE, 't', E_TREBUCHET},
        {E_DOCK, 'b', E_FISHING_BOAT},
        {E_DOCK, 'w', E_WARSHIP},
        {E_DOCK, 't', E_TRANSPORT},
    };
    for (const TrainMenuOption& option : options)
        if (option.producer == producer && keyMatches(ch, option.key)) return option.unit;
    return E_NONE;
}

static std::optional<ResearchId> researchMenuSelection(int ch) {
    static constexpr ResearchMenuOption options[] = {
        {'i', ResearchId::IronWeapons},
        {'c', ResearchId::Crossbows},
        {'p', ResearchId::Pikes},
        {'w', ResearchId::Counterweight},
        {'h', ResearchId::PlateHelm},
    };
    for (const ResearchMenuOption& option : options)
        if (keyMatches(ch, option.key)) return option.research;
    return std::nullopt;
}

static std::optional<MarketTradeType> tradeMenuSelection(int ch) {
    int count = 0;
    const MarketTradeDef* defs = marketTradeDefs(count);
    for (int i = 0; i < count; i++)
        if (keyMatches(ch, defs[i].key)) return defs[i].type;
    return std::nullopt;
}

static constexpr PlayerId kLocalPlayer = 0;

static void dispatchBuildCommand(PlayerId issuer, EntityType type, int x, int y, int endX = -1, int endY = -1) {
    Command command;
    command.issuer = issuer;
    if (endX >= 0 && endY >= 0) {
        command.payload = BuildLineCommand{ currentSelection(), type, {x, y}, {endX, endY} };
    } else {
        command.payload = BuildCommand{ currentSelection(), type, {x, y} };
    }
    dispatchCommand(g, command);
}

static void dispatchTrainCommand(PlayerId issuer, EntityType type) {
    Command command;
    command.issuer = issuer;
    command.payload = TrainCommand{ currentSelection(), type };
    dispatchCommand(g, command);
}

static void dispatchTileCommand(PlayerId issuer, CommandType type, int x, int y) {
    Command command;
    command.issuer = issuer;
    Selection selection = currentSelection();
    switch (type) {
    case CommandType::Select: command.payload = SelectCommand{ {x, y} }; break;
    case CommandType::SetRally: command.payload = SetRallyCommand{ selection, {x, y} }; break;
    case CommandType::AttackMove: command.payload = AttackMoveCommand{ selection, {x, y} }; break;
    case CommandType::Gather: command.payload = GatherCommand{ selection, {x, y} }; break;
    case CommandType::Move: command.payload = MoveCommand{ selection, {x, y} }; break;
    default: return;
    }
    dispatchCommand(g, command);
}

static void dispatchSimpleCommand(PlayerId issuer, CommandType type) {
    Command command;
    command.issuer = issuer;
    Selection selection = currentSelection();
    switch (type) {
    case CommandType::TogglePause: command.payload = TogglePauseCommand{}; break;
    case CommandType::Resign: command.payload = ResignCommand{}; break;
    case CommandType::EjectGarrison: command.payload = EjectGarrisonCommand{ selection }; break;
    case CommandType::ToggleGate: command.payload = ToggleGateCommand{ selection }; break;
    case CommandType::ToggleTrebuchetPacked: command.payload = ToggleTrebuchetPackedCommand{ selection }; break;
    case CommandType::ToggleDiagnostics: command.payload = ToggleDiagnosticsCommand{}; break;
    case CommandType::RevealMapDebug: command.payload = RevealMapDebugCommand{}; break;
    case CommandType::HoldPosition: command.payload = HoldPositionCommand{ selection }; break;
    case CommandType::Stop: command.payload = StopCommand{ selection }; break;
    default: return;
    }
    dispatchCommand(g, command);
}

static void dispatchSlotCommand(PlayerId issuer, CommandType type, int slot) {
    Command command;
    command.issuer = issuer;
    switch (type) {
    case CommandType::Save: command.payload = SaveCommand{ slot }; break;
    case CommandType::Load: command.payload = LoadCommand{ slot }; break;
    case CommandType::AssignControlGroup: command.payload = AssignControlGroupCommand{ currentSelection(), slot }; break;
    case CommandType::RecallControlGroup: command.payload = RecallControlGroupCommand{ slot }; break;
    default: return;
    }
    dispatchCommand(g, command);
}

static void moveCursorToSelected(PlayerId issuer) {
    std::optional<MapPos> pos = selectedEntityPosition(g, issuer);
    if (!pos) return;
    view.cursorX = pos->x;
    view.cursorY = pos->y;
}

static void handleInputForPlayer(int ch, PlayerId issuer) {
    InputIntentResult input = inputIntentFromKey(ch);
    if (input.intent == InputIntent::None) return;
    if (input.intent == InputIntent::Resign) {
        dispatchSimpleCommand(issuer, CommandType::Resign);
        return;
    }
    if (input.intent == InputIntent::Confirm && g.mode==M_GAME_OVER) {
        dispatchSimpleCommand(issuer, CommandType::Resign); return;
    }
    if (input.intent == InputIntent::TogglePause && (g.mode==M_NORMAL||g.mode==M_PAUSED)) {
        dispatchSimpleCommand(issuer, CommandType::TogglePause); return;
    }
    if (isInputBlockedByMode(g.mode)) return;

    // Build mode
    if (g.mode == M_BUILD_SELECT) {
        if (!selectedPeasantCanBuild(g, issuer)) { cancelInputMode(g); return; }
        if (ch == 27) { cancelInputMode(g); return; }
        EntityType tb = buildMenuSelection(ch);
        if (tb == E_WALL) {
            // Wall uses click-drag mode instead of point placement
            if (selectedPeasantCanBuild(g, issuer)) {
                startWallBuildMode(g);
                inputStatus("Click and drag to draw wall line...");
            }
            return;
        }
        if (tb == E_NONE) return;
        if (tb != E_NONE) { dispatchBuildCommand(issuer, tb, view.cursorX, view.cursorY); cancelInputMode(g); }
        return;
    }

    // Train mode
    if (g.mode == M_TRAIN_SELECT) {
        std::optional<EntityType> producer = selectedTrainProducerType(g, issuer);
        if (!producer) { cancelInputMode(g); return; }
        EntityType tt = trainMenuSelection(*producer, ch);
        if (tt != E_NONE) {
            dispatchTrainCommand(issuer, tt);
            // Keep train mode open so repeated unit keys queue more units. This
            // avoids P becoming Pause immediately after queueing a peasant.
            if (selectedProducerCanTrain(g, issuer)) setInputMode(g, M_TRAIN_SELECT);
        }
        if (ch == 27) cancelInputMode(g);
        return;
    }

    // Market resource trade.
    if (g.mode == M_MARKET_TRADE) {
        if (ch == 27) { cancelInputMode(g); return; }
        if (!selectedMarketCanTrade(g, issuer)) {
            cancelInputMode(g); return;
        }
        auto dispatchTrade = [&](MarketTradeType type) {
            Command command;
            command.issuer = issuer;
            command.payload = MarketTradeCommand{ currentSelection(), type };
            dispatchCommand(g, command);
            cancelInputMode(g);
        };
        std::optional<MarketTradeType> trade = tradeMenuSelection(ch);
        if (trade) dispatchTrade(*trade);
        return;
    }

    // Wall drag mode
    if (g.mode == M_WALL_DRAG) {
        if (ch == 27) { cancelInputMode(g); view.dragging = false; return; }
        // Cursor movement — preview updates in render
        if (ch == KEY_UP)    { view.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { view.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { view.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { view.cursorX++; goto clamp; }
        // Space / Enter: set start (first press) or confirm placement (second press)
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            if (!view.dragging) {
                view.dragging = true;
                view.wallDragX = view.cursorX; view.wallDragY = view.cursorY;
                inputStatus("Wall start set — move cursor then press Space/Enter to place");
            } else {
                if (selectedPeasantCanBuild(g, issuer)) {
                    dispatchBuildCommand(issuer, E_WALL, view.wallDragX, view.wallDragY, view.cursorX, view.cursorY);
                }
                view.dragging = false; cancelInputMode(g);
            }
            goto clamp;
        }
        if (ch == KEY_MOUSE) {
            MEVENT me;
            if (getmouse(&me) != OK) goto clamp;
            ViewportCell cell = viewportCellAt(view, me.x, me.y, 2);
            if (!cell.inMap) goto clamp;
            int mapX = cell.x;
            int mapY = cell.y;
            view.cursorX = mapX; view.cursorY = mapY;
            if (me.bstate & BUTTON1_PRESSED) {
                view.dragging = true;
                view.wallDragX = mapX; view.wallDragY = mapY;
            } else if (me.bstate & (BUTTON1_RELEASED | BUTTON1_CLICKED)) {
                if (view.dragging || (me.bstate & BUTTON1_CLICKED)) {
                    if (selectedPeasantCanBuild(g, issuer)) {
                        dispatchBuildCommand(issuer, E_WALL, view.wallDragX, view.wallDragY, mapX, mapY);
                    }
                }
                view.dragging = false;
                cancelInputMode(g);
            }
        }
        goto clamp;
    }

    // Rally point selection — next click/Enter sets the selected building's rally.
    if (g.mode == M_RALLY_SET) {
        if (ch == 27) { cancelInputMode(g); return; }
        if (ch == KEY_UP)    { view.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { view.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { view.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { view.cursorX++; goto clamp; }
        auto commit = [issuer](int tx, int ty) {
            dispatchTileCommand(issuer, CommandType::SetRally, tx, ty);
            cancelInputMode(g);
        };
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(view.cursorX, view.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            ViewportCell cell = viewportCellAt(view, me.x, me.y, 2);
            if (cell.inMap) {
                view.cursorX = cell.x; view.cursorY = cell.y;
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED)) commit(cell.x, cell.y);
            }
        }
        goto clamp;
    }

    // Attack-move target selection.
    if (g.mode == M_ATTACK_MOVE) {
        if (ch == 27) { cancelInputMode(g); return; }
        if (ch == KEY_UP)    { view.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { view.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { view.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { view.cursorX++; goto clamp; }
        auto commit = [issuer](int tx, int ty) {
            dispatchTileCommand(issuer, CommandType::AttackMove, tx, ty);
            cancelInputMode(g);
        };
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(view.cursorX, view.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            ViewportCell cell = viewportCellAt(view, me.x, me.y, 2);
            if (cell.inMap) {
                view.cursorX = cell.x; view.cursorY = cell.y;
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED)) commit(cell.x, cell.y);
            }
        }
        goto clamp;
    }

    // Research selection from the blacksmith.
    if (g.mode == M_RESEARCH_SELECT) {
        if (ch == 27) { cancelInputMode(g); return; }
        if (!selectedBlacksmithCanResearch(g, issuer)) {
            cancelInputMode(g); return;
        }
        auto dispatchResearch = [&](ResearchId id) {
            Command command;
            command.issuer = issuer;
            command.payload = ResearchCommand{ currentSelection(), id };
            dispatchCommand(g, command);
            cancelInputMode(g);
        };
        std::optional<ResearchId> research = researchMenuSelection(ch);
        if (research) dispatchResearch(*research);
        return;
    }

    switch (input.intent) {
    case InputIntent::CursorUp:    view.cursorY--; break;
    case InputIntent::CursorDown:  view.cursorY++; break;
    case InputIntent::CursorLeft:  view.cursorX--; break;
    case InputIntent::CursorRight: view.cursorX++; break;

    // Fast cursor pan — 10 tiles per press. Crossing a 140x90 map takes
    // dozens of presses otherwise. Shift+arrows on most terminals.
    case InputIntent::CursorFastUp:    view.cursorY -= 10; break;
    case InputIntent::CursorFastDown:  view.cursorY += 10; break;
    case InputIntent::CursorFastLeft:  view.cursorX -= 10; break;
    case InputIntent::CursorFastRight: view.cursorX += 10; break;

    case InputIntent::ToggleHelp:
        inputStatus(toggleHelpOverlay(g) ? "Help open." : "Help closed.");
        break;

    case InputIntent::ToggleDiagnosticsOrTrebuchet:
        {
            if (selectedTrebuchetCanToggle(g, issuer)) {
                dispatchSimpleCommand(issuer, CommandType::ToggleTrebuchetPacked);
            } else {
                dispatchSimpleCommand(issuer, CommandType::ToggleDiagnostics);
            }
        }
        break;

    case InputIntent::Save:
        dispatchSlotCommand(issuer, CommandType::Save, input.slot);
        break;

    case InputIntent::Load:
        dispatchSlotCommand(issuer, CommandType::Load, input.slot);
        break;

    case InputIntent::Select: {
        dispatchTileCommand(issuer, CommandType::Select, view.cursorX, view.cursorY);
        break;
    }

    case InputIntent::Confirm: {
        dispatchCommand(g, resolveContextCommand(g, issuer, currentSelection(), {view.cursorX, view.cursorY}));
        break;
    }

    case InputIntent::BuildMenu: {
        if (selectedPeasantCanBuild(g, issuer)) {
            setInputMode(g, M_BUILD_SELECT);
            inputStatus("Select building to place at cursor...");
        } else inputStatus("Select a peasant first!");
        break;
    }

    case InputIntent::TrainMenu: {
        InputTrainMenuEligibility eligibility = trainMenuEligibilityForSelected(g, issuer);
        if (eligibility == InputTrainMenuEligibility::CanTrain) {
            setInputMode(g, M_TRAIN_SELECT);
            inputStatus("Select unit to train...");
        } else if (eligibility == InputTrainMenuEligibility::UnsupportedBuilding) {
            inputStatus("This building can't train.");
        } else inputStatus("Select a production building!");
        break;
    }

    // Eject garrison from selected building or transport
    case InputIntent::EjectGarrison: {
        dispatchSimpleCommand(issuer, CommandType::EjectGarrison);
        break;
    }

    // Rally point (production buildings) / research menu (blacksmith)
    case InputIntent::RallyResearchTradeMenu: {
        InputUtilityMode utilityMode = utilityModeForSelectedBuilding(g, issuer);
        if (utilityMode == InputUtilityMode::None) {
            inputStatus("Select a production building first.");
            break;
        }
        if (utilityMode == InputUtilityMode::MarketTrade) {
            setInputMode(g, M_MARKET_TRADE);
            inputStatus("Trade: [G] 40g->30w  [W] 40w->30g  [F] 50g->30f  [V] 40f->30g");
        } else if (utilityMode == InputUtilityMode::Research) {
            setInputMode(g, M_RESEARCH_SELECT);
            inputStatus("Research: [I]ron [C]rossbows [P]ikes [W]eight [H]elm  [Esc]");
        } else if (utilityMode == InputUtilityMode::Rally) {
            setInputMode(g, M_RALLY_SET);
            inputStatus("Click a tile (or move cursor + Enter) to set rally. [Esc]");
        } else {
            inputStatus("Nothing to rally or research here.");
        }
        break;
    }

    // Cycle to the next idle peasant
    case InputIntent::CycleIdleWorker: {
        WorldIndex world = buildWorldIndex(g);
        Entity* pick = selectNextIdleWorker(g, world, issuer, g.selectedId);
        if (pick) {
            view.cursorX = pick->x; view.cursorY = pick->y;
            inputStatus("Idle peasant selected");
        } else inputStatus("No idle peasants");
        break;
    }

    // Gate toggle: cycle auto -> locked-open -> locked-closed -> auto
    case InputIntent::ToggleGate: {
        dispatchSimpleCommand(issuer, CommandType::ToggleGate);
        break;
    }

    // Debug: reveal entire map (Shift+S)
    case InputIntent::RevealMapDebug: {
        dispatchSimpleCommand(issuer, CommandType::RevealMapDebug);
        break;
    }

    // 'A' is overloaded:
    //   with no selection or non-military selection → select all military
    //   with military selected → enter attack-move mode (next click = a-move target)
    case InputIntent::AttackMoveOrSelectArmy: {
        WorldIndex world = buildWorldIndex(g);
        bool hasMilitarySel = selectionContainsMilitary(g, world, issuer, currentSelection());
        if (hasMilitarySel) {
            setInputMode(g, M_ATTACK_MOVE);
            inputStatus("Attack-move: click destination. [Esc] cancel");
        } else {
            int count = selectAllMilitary(g, world, issuer);
            std::optional<MapPos> pos = selectedEntityPosition(g, issuer);
            if (pos) {
                view.cursorX = pos->x;
                view.cursorY = pos->y;
            }
            if (count == 0) inputStatus("No military units!");
            else inputStatus(std::to_string(count) + " military units selected");
        }
        break;
    }

    // Hold position — stop and ignore auto-aggro until explicitly ordered.
    case InputIntent::HoldPosition: {
        dispatchSimpleCommand(issuer, CommandType::HoldPosition);
        break;
    }

    // Enter group assign mode
    case InputIntent::GroupAssign: {
        WorldIndex world = buildWorldIndex(g);
        if (beginControlGroupAssignment(g, world, issuer)) {
            inputStatus("Press [1]-[9] to assign group...");
        }
        break;
    }

    // Control groups 1-9
    case InputIntent::ControlGroup: {
        int idx = input.slot;
        if (controlGroupAssignmentPending(g)) {
            dispatchSlotCommand(issuer, CommandType::AssignControlGroup, idx);
        } else {
            dispatchSlotCommand(issuer, CommandType::RecallControlGroup, idx);
            moveCursorToSelected(issuer);
        }
        break;
    }

    // Cycle through own units
    case InputIntent::CycleUnit: {
        WorldIndex world = buildWorldIndex(g);
        Entity* selected = selectNextUnit(g, world, issuer, g.selectedId);
        if (selected) {
            view.cursorX = selected->x;
            view.cursorY = selected->y;
        }
        break;
    }

    // Home to town hall
    case InputIntent::HomeBase: {
        WorldIndex world = buildWorldIndex(g);
        Entity* home = selectHomeBase(g, world, issuer);
        if (home) {
            view.cursorX = home->x + 1;
            view.cursorY = home->y + 1;
        }
        break;
    }

    case InputIntent::Cancel:
        clearSelection(g);
        cancelInputMode(g);
        break;

    case InputIntent::Mouse: {
        MEVENT me;
        if (getmouse(&me) != OK) break;
        // Minimap click → jump viewport. Minimap sits at panelX+1..+mmW, mmY..+mmH.
        {
            int maxY2, maxX2; getmaxyx(stdscr, maxY2, maxX2); (void)maxY2;
            bool minimapClick = (me.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED
                              | BUTTON3_CLICKED | BUTTON3_PRESSED)) != 0;
            if (handleMinimapClick(view, maxX2, me.x, me.y, minimapClick)) {
                break;
            }
        }
        ViewportCell cell = viewportCellAt(view, me.x, me.y, 2);
        if (!cell.inMap) { view.dragging = false; break; }
        int mapX = cell.x;
        int mapY = cell.y;

        // Hover-track the cursor, but only when the mouse actually crossed into a new map
        // cell. Without this, every stale ncurses motion event would yank the cursor back
        // to the OS mouse position, fighting keyboard arrow input. Clicks/drags still pin
        // the cursor regardless of last-cell state.
        static int lastMx = -9999, lastMy = -9999;
        bool clickEvt = (me.bstate & (BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON1_CLICKED
                                    |BUTTON1_DOUBLE_CLICKED|BUTTON3_CLICKED|BUTTON3_PRESSED)) != 0;
        if (clickEvt || mapX != lastMx || mapY != lastMy) {
            view.cursorX = mapX; view.cursorY = mapY;
            lastMx = mapX; lastMy = mapY;
        }

        if (me.bstate & BUTTON1_DOUBLE_CLICKED) {
            // Select all of clicked unit type within the current viewport.
            Command command;
            command.issuer = issuer;
            command.payload = SelectAllOfTypeInViewCommand{ {mapX, mapY} };
            dispatchCommand(g, command);
            view.dragging = false;
            break;
        }
        if (me.bstate & BUTTON1_PRESSED) {
            // Start of left-button drag/click
            view.dragging    = true;
            view.dragStartX  = mapX;
            view.dragStartY  = mapY;
        }
        else if (me.bstate & BUTTON1_RELEASED) {
            if (view.dragging) {
                view.dragging = false;
                bool moved = (std::abs(mapX - view.dragStartX) + std::abs(mapY - view.dragStartY)) > 1;
                if (moved) {
                    // Box select: all own units inside the rectangle
                    Command command;
                    command.issuer = issuer;
                    command.payload = BoxSelectCommand{ {view.dragStartX, view.dragStartY}, {mapX, mapY} };
                    dispatchCommand(g, command);
                } else {
                    // Click: select entity at cursor
                    Command command;
                    command.issuer = issuer;
                    command.payload = SelectCommand{ {mapX, mapY} };
                    dispatchCommand(g, command);
                }
            }
        }
        else if (me.bstate & BUTTON1_CLICKED) {
            // Terminals that report CLICKED instead of PRESSED+RELEASED
            view.dragging = false;
            Command command;
            command.issuer = issuer;
            command.payload = SelectCommand{ {mapX, mapY} };
            dispatchCommand(g, command);
        }
        else if (me.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) {
            // Right click: issue command at cursor position
            view.dragging = false;
            dispatchCommand(g, resolveContextCommand(g, issuer, currentSelection(), {mapX, mapY}));
        }
        // All other events (pure movement): cursor already updated above
        break;
    }
    case InputIntent::None:
    case InputIntent::TogglePause:
    case InputIntent::Resign:
    case InputIntent::Context:
        break;
    }

    clamp:
    clampCursorToMap(view);
}

void handleInput(int ch) {
    handleInputForPlayer(ch, kLocalPlayer);
}
