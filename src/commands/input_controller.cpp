#include "realm.h"
#include "view_state.h"
#include "input_keys.h"

#include "commands/command.h"
#include "commands/command_runner.h"
#include "commands/input_intent.h"
#include "commands/input_mode_controller.h"
#include "core/build_service.h"
#include "core/game_events.h"
#include "core/market_service.h"
#include "core/production_service.h"

#include <cctype>
#include <climits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

static bool keyMatches(int ch, char key) {
    return ch >= 0 && ch <= UCHAR_MAX && std::tolower((unsigned char)ch) == key;
}

static EntityType buildMenuSelection(int ch) {
    int count = 0;
    const BuildRule* rules = buildRules(count);
    for (int i = 0; i < count; i++)
        if (keyMatches(ch, rules[i].menuHotkey)) return rules[i].buildingType;
    return E_NONE;
}

static EntityType productionRuleUnitForHotkey(EntityType producer, int ch) {
    const ProductionRule* rule = productionRule(producer);
    if (!rule || !rule->menuHotkeys) return E_NONE;
    for (int i = 0; i < rule->allowedCount && rule->menuHotkeys[i] != '\0'; i++)
        if (keyMatches(ch, rule->menuHotkeys[i])) return rule->allowedUnits[i];
    return E_NONE;
}

static std::optional<ResearchId> researchMenuSelection(int ch) {
    int count = 0;
    const ResearchDef* defs = researchDefs(count);
    for (int i = 0; i < count; i++)
        if (keyMatches(ch, defs[i].menuHotkey)) return defs[i].id;
    return std::nullopt;
}

static std::string researchMenuPrompt() {
    int count = 0;
    const ResearchDef* defs = researchDefs(count);
    std::string prompt = "Research:";
    for (int i = 0; i < count; i++) {
        prompt += " [";
        prompt += (char)std::toupper((unsigned char)defs[i].menuHotkey);
        prompt += ']';
        prompt += defs[i].menuLabel;
    }
    prompt += "  [Esc]";
    return prompt;
}

static std::optional<MarketTradeType> tradeMenuSelection(int ch) {
    int count = 0;
    const MarketTradeDef* defs = marketTradeDefs(count);
    for (int i = 0; i < count; i++)
        if (keyMatches(ch, defs[i].key)) return defs[i].type;
    return std::nullopt;
}

static constexpr PlayerId kLocalPlayer = 0;

static Command inputCommand(PlayerId issuer, CommandPayload payload) {
    Command command;
    command.issuer = issuer;
    command.payload = std::move(payload);
    return command;
}

static bool mouseShiftDown(const MEVENT& event) {
#ifdef BUTTON_SHIFT
    return (event.bstate & BUTTON_SHIFT) != 0;
#else
    (void)event;
    return false;
#endif
}

static bool selectionHasLandUnit(Game& game, const WorldIndex& world, PlayerId issuer) {
    for (int id : currentSelection(game).ids) {
        Entity* entity = findEntity(game, world, id);
        if (entity && entity->alive && entity->owner == issuer && isUnit(entity->type) && !isNaval(entity->type))
            return true;
    }
    return false;
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
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, ResignCommand{}));
        return;
    }
    if (input.intent == InputIntent::Confirm && g.mode==M_GAME_OVER) {
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, ResignCommand{})); return;
    }
    if (input.intent == InputIntent::TogglePause && (g.mode==M_NORMAL||g.mode==M_PAUSED)) {
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, TogglePauseCommand{})); return;
    }
    if (isInputBlockedByMode(g.mode)) return;

    std::unique_ptr<WorldIndex> inputWorld;
    auto worldForInput = [&]() -> const WorldIndex& {
        if (!inputWorld) inputWorld = std::make_unique<WorldIndex>(buildWorldIndex(g));
        return *inputWorld;
    };
    auto status = [issuer](const std::string& message) {
        gameEvents().emit({ GameEventType::StatusMessage, issuer, -1, { -1, -1 }, message, 0 });
    };

    // Build mode
    if (g.mode == M_BUILD_SELECT) {
        if (!selectedPeasantCanBuild(g, issuer)) { cancelInputMode(g); return; }
        if (ch == 27) { cancelInputMode(g); return; }
        EntityType tb = buildMenuSelection(ch);
        if (tb == E_WALL) {
            // Wall uses click-drag mode instead of point placement
            if (selectedPeasantCanBuild(g, issuer)) {
                startWallBuildMode(g);
                status("Click and drag to draw wall line...");
            }
            return;
        }
        if (tb == E_NONE) return;
        if (tb != E_NONE) {
            g.local.buildPending = tb;
            setInputMode(g, M_BUILD_PLACE);
            status(std::string("Place ") + STATS[tb].name + ": arrows/mouse then Enter. [Esc]");
        }
        return;
    }

    // Build placement mode: cursor moves freely with a ghost footprint preview.
    if (g.mode == M_BUILD_PLACE) {
        if (!selectedPeasantCanBuild(g, issuer)) { g.local.buildPending = E_NONE; cancelInputMode(g); return; }
        if (ch == 27) {
            g.local.buildPending = E_NONE;
            cancelInputMode(g);
            status("Build cancelled.");
            return;
        }
        if (ch == KEY_UP)    { view.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { view.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { view.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { view.cursorX++; goto clamp; }
        auto commitBuild = [&](int tx, int ty) {
            EntityType bt = g.local.buildPending;
            dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, BuildCommand{ currentSelection(g), bt, {tx, ty} }));
            g.local.buildPending = E_NONE;
            cancelInputMode(g);
        };
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            commitBuild(view.cursorX, view.cursorY);
            goto clamp;
        }
        if (ch == KEY_MOUSE) {
            MEVENT me;
            if (getmouse(&me) != OK) goto clamp;
            ViewportCell cell = viewportCellAt(view, me.x, me.y, 2);
            if (cell.inMap) { view.cursorX = cell.x; view.cursorY = cell.y; }
            if (cell.inMap && (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED))) {
                commitBuild(cell.x, cell.y);
            } else if (me.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) {
                g.local.buildPending = E_NONE;
                cancelInputMode(g);
                status("Build cancelled.");
            }
            goto clamp;
        }
        return;
    }

    // Train mode
    if (g.mode == M_TRAIN_SELECT) {
        std::optional<EntityType> producer = selectedTrainProducerType(g, issuer);
        if (!producer) { cancelInputMode(g); return; }
        EntityType tt = productionRuleUnitForHotkey(*producer, ch);
        if (tt != E_NONE) {
            dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, TrainCommand{ currentSelection(g), tt }));
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
            command.payload = MarketTradeCommand{ currentSelection(g), type };
            dispatchCommandForLocalGame(g, gameEvents(), command);
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
                status("Wall start set — move cursor then press Space/Enter to place");
            } else {
                if (selectedPeasantCanBuild(g, issuer)) {
                    dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, BuildLineCommand{ currentSelection(g), E_WALL, {view.wallDragX, view.wallDragY}, {view.cursorX, view.cursorY} }));
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
                        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, BuildLineCommand{ currentSelection(g), E_WALL, {view.wallDragX, view.wallDragY}, {mapX, mapY} }));
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
            dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, SetRallyCommand{ currentSelection(g), {tx, ty} }));
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
            dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, AttackMoveCommand{ currentSelection(g), {tx, ty} }));
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

    // Patrol target selection.
    if (g.mode == M_PATROL_SET) {
        if (ch == 27) {
            cancelInputMode(g);
            status("Patrol cancelled.");
            return;
        }
        if (ch == KEY_UP)    { view.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { view.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { view.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { view.cursorX++; goto clamp; }
        auto commit = [issuer](int tx, int ty) {
            dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, PatrolCommand{ currentSelection(g), {tx, ty} }));
            cancelInputMode(g);
        };
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(view.cursorX, view.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            ViewportCell cell = viewportCellAt(view, me.x, me.y, 2);
            if (cell.inMap) {
                view.cursorX = cell.x; view.cursorY = cell.y;
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED | BUTTON3_PRESSED)) commit(cell.x, cell.y);
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
            command.payload = ResearchCommand{ currentSelection(g), id };
            dispatchCommandForLocalGame(g, gameEvents(), command);
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
        status(toggleHelpOverlay(g) ? "Help open." : "Help closed.");
        break;

    case InputIntent::ToggleDiagnosticsOrTrebuchet:
        {
            if (selectedTrebuchetCanToggle(g, issuer)) {
                dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, ToggleTrebuchetPackedCommand{ currentSelection(g) }));
            } else {
                dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, ToggleDiagnosticsCommand{}));
            }
        }
        break;

    case InputIntent::Save:
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, SaveCommand{ input.slot }));
        break;

    case InputIntent::Load:
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, LoadCommand{ input.slot }));
        break;

    case InputIntent::Select: {
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, SelectCommand{ {view.cursorX, view.cursorY} }));
        break;
    }

    case InputIntent::Confirm: {
        const WorldIndex& world = worldForInput();
        Command command = resolveContextCommand(g, world, issuer, currentSelection(g), {view.cursorX, view.cursorY});
        dispatchCommandForLocalGame(g, gameEvents(), command);
        break;
    }

    case InputIntent::BuildMenu: {
        if (selectedPeasantCanBuild(g, issuer)) {
            setInputMode(g, M_BUILD_SELECT);
            status("Select building to place at cursor...");
        } else status("Select a peasant first!");
        break;
    }

    case InputIntent::TrainMenu: {
        InputTrainMenuEligibility eligibility = trainMenuEligibilityForSelected(g, issuer);
        if (eligibility == InputTrainMenuEligibility::CanTrain) {
            setInputMode(g, M_TRAIN_SELECT);
            status("Select unit to train...");
        } else if (eligibility == InputTrainMenuEligibility::UnsupportedBuilding) {
            status("This building can't train.");
        } else status("Select a production building!");
        break;
    }

    // Eject garrison from selected building or transport
    case InputIntent::EjectGarrison: {
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, EjectGarrisonCommand{ currentSelection(g) }));
        break;
    }

    // Rally point (production buildings) / research menu (blacksmith)
    case InputIntent::RallyResearchTradeMenu: {
        InputUtilityMode utilityMode = utilityModeForSelectedBuilding(g, issuer);
        if (utilityMode == InputUtilityMode::None) {
            status("Select a production building first.");
            break;
        }
        if (utilityMode == InputUtilityMode::MarketTrade) {
            setInputMode(g, M_MARKET_TRADE);
            status("Trade: [G] 40g->30w  [W] 40w->30g  [F] 50g->30f  [V] 40f->30g");
        } else if (utilityMode == InputUtilityMode::Research) {
            setInputMode(g, M_RESEARCH_SELECT);
            status(researchMenuPrompt());
        } else if (utilityMode == InputUtilityMode::Rally) {
            setInputMode(g, M_RALLY_SET);
            status("Click a tile (or move cursor + Enter) to set rally. [Esc]");
        } else {
            status("Nothing to rally or research here.");
        }
        break;
    }

    // Cycle to the next idle peasant
    case InputIntent::CycleIdleWorker: {
        const WorldIndex& world = worldForInput();
        Entity* pick = selectNextIdleWorker(g, world, issuer, g.local.selectedId);
        if (pick) {
            view.cursorX = pick->x; view.cursorY = pick->y;
            status("Idle peasant selected");
        } else status("No idle peasants");
        break;
    }

    // Gate toggle: cycle auto -> locked-open -> locked-closed -> auto
    case InputIntent::ToggleGate: {
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, ToggleGateCommand{ currentSelection(g) }));
        break;
    }

    // Debug: reveal entire map (Shift+S)
    case InputIntent::RevealMapDebug: {
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, RevealMapDebugCommand{}));
        break;
    }

    // 'A' is overloaded:
    //   with no selection or non-military selection → select all military
    //   with military selected → enter attack-move mode (next click = a-move target)
    case InputIntent::AttackMoveOrSelectArmy: {
        const WorldIndex& world = worldForInput();
        bool hasMilitarySel = selectionContainsMilitary(g, world, issuer, currentSelection(g));
        if (hasMilitarySel) {
            setInputMode(g, M_ATTACK_MOVE);
            status("Attack-move: click destination. [Esc] cancel");
        } else {
            int count = selectAllMilitary(g, world, issuer);
            std::optional<MapPos> pos = selectedEntityPosition(g, issuer);
            if (pos) {
                view.cursorX = pos->x;
                view.cursorY = pos->y;
            }
            if (count == 0) status("No military units!");
            else status(std::to_string(count) + " military units selected");
        }
        break;
    }

    case InputIntent::Patrol: {
        const WorldIndex& world = worldForInput();
        if (selectionHasLandUnit(g, world, issuer)) {
            setInputMode(g, M_PATROL_SET);
            status("Patrol: click target. [Esc] cancel");
        } else {
            status("Select land units to patrol.");
        }
        break;
    }

    // Hold position — stop and ignore auto-aggro until explicitly ordered.
    case InputIntent::HoldPosition: {
        dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, HoldPositionCommand{ currentSelection(g) }));
        break;
    }

    // Enter group assign mode
    case InputIntent::GroupAssign: {
        const WorldIndex& world = worldForInput();
        if (beginControlGroupAssignment(g, world, issuer)) {
            status("Press [1]-[9] to assign group...");
        }
        break;
    }

    // Control groups 1-9
    case InputIntent::ControlGroup: {
        int idx = input.slot;
        if (controlGroupAssignmentPending(g)) {
            dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, AssignControlGroupCommand{ currentSelection(g), idx }));
        } else {
            dispatchCommandForLocalGame(g, gameEvents(), inputCommand(issuer, RecallControlGroupCommand{ idx }));
            moveCursorToSelected(issuer);
        }
        break;
    }

    // Cycle through own units
    case InputIntent::CycleUnit: {
        const WorldIndex& world = worldForInput();
        Entity* selected = selectNextUnit(g, world, issuer, g.local.selectedId);
        if (selected) {
            view.cursorX = selected->x;
            view.cursorY = selected->y;
        }
        break;
    }

    // Home to town hall
    case InputIntent::HomeBase: {
        const WorldIndex& world = worldForInput();
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
        if (!clickEvt) {
            const int edgeMargin = 2;
            const int edgeStep = 2;
            int mapSX = me.x;
            int mapSY = me.y - 2;
            int dx = 0, dy = 0;
            if (mapSX < edgeMargin) dx = -edgeStep;
            else if (mapSX >= view.viewW - edgeMargin) dx = edgeStep;
            if (mapSY < edgeMargin) dy = -edgeStep;
            else if (mapSY >= view.viewH - edgeMargin) dy = edgeStep;
            if (dx || dy) {
                view.viewX = std::max(0, std::min(view.viewX + dx, MAP_W - view.viewW));
                view.viewY = std::max(0, std::min(view.viewY + dy, MAP_H - view.viewH));
            }
        }
        bool shift = mouseShiftDown(me);

        if (me.bstate & BUTTON1_DOUBLE_CLICKED) {
            // Select all of clicked unit type within the current viewport.
            Command command;
            command.issuer = issuer;
            command.payload = SelectAllOfTypeInViewCommand{ {mapX, mapY} };
            dispatchCommandForLocalGame(g, gameEvents(), command);
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
                    command.payload = BoxSelectCommand{ {view.dragStartX, view.dragStartY}, {mapX, mapY}, shift };
                    dispatchCommandForLocalGame(g, gameEvents(), command);
                } else {
                    // Click: select entity at cursor
                    Command command;
                    command.issuer = issuer;
                    command.payload = SelectCommand{ {mapX, mapY}, shift };
                    dispatchCommandForLocalGame(g, gameEvents(), command);
                }
            }
        }
        else if (me.bstate & BUTTON1_CLICKED) {
            // Terminals that report CLICKED instead of PRESSED+RELEASED
            view.dragging = false;
            Command command;
            command.issuer = issuer;
            command.payload = SelectCommand{ {mapX, mapY}, shift };
            dispatchCommandForLocalGame(g, gameEvents(), command);
        }
        else if (me.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) {
            // Right click: issue command at cursor position
            view.dragging = false;
            const WorldIndex& world = worldForInput();
            Command command = shift
                ? inputCommand(issuer, WaypointCommand{ currentSelection(g), {mapX, mapY} })
                : resolveContextCommand(g, world, issuer, currentSelection(g), {mapX, mapY});
            dispatchCommandForLocalGame(g, gameEvents(), command);
        }
        // All other events (pure movement): cursor already updated above
        break;
    }
    case InputIntent::None:
    case InputIntent::TogglePause:
    case InputIntent::Resign:
        break;
    }

    clamp:
    clampCursorToMap(view);
}

void handleInput(int ch) {
    handleInputForPlayer(ch, kLocalPlayer);
}

