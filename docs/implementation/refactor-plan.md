# Realm Refactor Implementation Plan

This plan is ordered for implementation. Each phase should be completed, tested, and committed before moving to the next one. The main goal is to finish the partially migrated architecture: commands, input, AI, definitions, rendering, validation, save/load, and query/indexing should each have a clear responsibility.

---

## Core target architecture

The final architecture should look like this:

```text
platform/
  terminal, SDL, web, mobile adapters
        ↓
input/
  raw input → InputIntent → mode handling
        ↓
commands/
  resolve intent → validate command → apply command
        ↓
domain services/
  build, train, research, gather, combat, garrison, market, selection
        ↓
core/
  Game, Entity, Player, Terrain, WorldIndex, definitions
        ↓
sim/
  movement, combat ticking, production ticking, economy ticking, weather/seasons
        ↓
ai/
  sensors → planners → commands
        ↓
render/
  RenderModel → ASCII / SDL / web renderers
        ↓
save/
  versioned save schema + migrations
```

The architectural rule is:

```text
Platform code reads devices.
Input code creates intents.
Command code applies player/AI actions.
Simulation code advances time.
Render code observes game state.
AI code plans actions but does not mutate directly.
Save code serializes and migrates state.
```

No layer should casually mutate `g` except through its assigned boundary.

---

# Phase 0 — Establish the safety net first

Do this before any large refactor.

## Goal

Create enough tests, build scripts, and guardrails that later refactors can be verified without manual playtesting every change.

## Implementation checklist

* [ ] Add a repeatable test/build command, for example:

```text
make test
make run-terminal
make run-sdl
ctest
```

or equivalent commands already used by the project.

* [ ] Add a minimal test harness that can:

  * [ ] Initialize a fresh `Game`.
  * [ ] Spawn entities.
  * [ ] Set resources.
  * [ ] Issue commands.
  * [ ] Tick simulation.
  * [ ] Assert entity/player state.
  * [ ] Run with deterministic RNG seed.

* [ ] Add regression tests for current critical behavior:

  * [ ] Select entity at tile.
  * [ ] Box select units.
  * [ ] Move command.
  * [ ] Attack command.
  * [ ] Gather command.
  * [ ] Build command.
  * [ ] Train command.
  * [ ] Research command.
  * [ ] Garrison/eject command.
  * [ ] Save and load a small game.
  * [ ] `validateGameState()` passes after each test.

* [ ] Add a simple golden scenario fixture:

  * [ ] One human Town Hall.
  * [ ] One peasant.
  * [ ] One enemy unit.
  * [ ] One resource patch.
  * [ ] Known map seed.

* [ ] Add compiler warnings suitable for the current codebase:

  * [ ] `-Wall`
  * [ ] `-Wextra`
  * [ ] `-Wswitch`
  * [ ] `-Wreturn-type`
  * [ ] Optional later: `-Werror` after warnings are reduced.

* [ ] Add a short `docs/refactor-roadmap.md` containing this plan or a condensed version.

## Done when

* [ ] The project builds from a clean checkout.
* [ ] The test harness runs in one command.
* [ ] At least 10 baseline tests exist.
* [ ] Tests can initialize a deterministic game.
* [ ] `validateGameState()` is exercised by tests.
* [ ] No large behavior-changing refactor has started yet.

---

# Phase 1 — Fix known correctness bugs and dangerous shims

This phase fixes concrete bugs before the broader architecture work.

---

## 1.1 Fix `x` / `X` input conflict

## Current problem

`handleInput()` exits the app immediately when `x` or `X` is pressed, but later in the same input handler `x` / `X` is intended to mean “hold position.” The help table also advertises `X` as exit, while gameplay wants `X` as hold position. This is a direct command conflict in the uploaded input/controller code. 

## Required change

Choose one of these policies:

```text
Preferred:
  X = Hold position
  Q = Resign / return to menu
  Ctrl+Q or menu action = Exit app

Acceptable:
  X = Exit only outside active gameplay
  X = Hold position during gameplay
```

## Implementation checklist

* [ ] Remove the early unconditional `if (ch == 'x' || ch == 'X') exit(0)` from `handleInput()`.
* [ ] Keep `x` / `X` for hold position in active gameplay.
* [ ] Update `gameplayCommands()` help text:

  * [ ] Remove or change `X: Exit`.
  * [ ] Add `X: Hold position`.
  * [ ] Ensure `Q` clearly means resign/return to menu.
* [ ] Add a test:

  * [ ] Select a unit.
  * [ ] Press/dispatch hold position.
  * [ ] Assert unit enters idle state.
  * [ ] Assert `holdPosition == 1`.
  * [ ] Assert the app/game does not exit.

## Done when

* [ ] `X` no longer exits during gameplay.
* [ ] `X` consistently holds selected units.
* [ ] Help text matches actual behavior.
* [ ] Test coverage exists.

---

## 1.2 Fix wall-line building

## Current problem

Wall-line building is a legacy shim inside `dispatchCommand()`:

```text
groupIndex packs end coordinates
owner is hardcoded to 0
wood cost is hardcoded to 20
walls are spawned directly
orderBuild() is bypassed
```

That causes ownership, cost, validation, and balance drift risks. 

## Required change

Wall-line building must use the same build rules as normal building placement.

## Implementation checklist

* [ ] Add a typed command payload for wall lines.

Recommended interim shape:

```cpp
struct BuildLinePayload {
    EntityId builderId;
    EntityType type;
    MapPos start;
    MapPos end;
};
```

* [ ] Stop packing wall end coordinates into `groupIndex`.
* [ ] Use the builder’s actual owner.
* [ ] Use `STATS[E_WALL].costWood` and `STATS[E_WALL].costGold`.
* [ ] Use `canPlace(E_WALL, x, y, builder.owner)` for every wall segment.
* [ ] Do not directly assume player `0`.
* [ ] Do not directly hardcode `20` wood.
* [ ] Decide and implement line-affordability behavior:

  * [ ] Either build as many valid segments as affordable.
  * [ ] Or pre-calculate total cost and reject if unaffordable.
* [ ] Prefer pre-calculation for clearer UX.
* [ ] After successful placement, order the builder to help the first wall segment.
* [ ] Emit status through the same mechanism as other build commands.

## Tests

* [ ] Human peasant builds wall line and pays correct total cost.
* [ ] AI/non-human peasant builds wall line and pays that AI player’s resources.
* [ ] Wall line fails if builder cannot afford it.
* [ ] Wall line skips or rejects invalid terrain according to chosen policy.
* [ ] No wall placement uses hardcoded player `0`.
* [ ] No wall placement uses hardcoded wall cost.

## Done when

* [ ] Wall-line placement is owner-aware.
* [ ] Wall-line placement uses canonical costs.
* [ ] Wall-line placement goes through shared build validation.
* [ ] `groupIndex` is no longer used for wall coordinates.

---

## 1.3 Make AI research use the same path as player research

## Current problem

AI blacksmith research writes directly to:

```cpp
smith.researching
smith.researchProgress
smith.researchTime
```

It also uses shorter research times and does not spend resources in the same way as the player path. The player path validates resources and uses separate costs/times inside input handling. 

## Required change

All research must go through a single research service or command path.

## Implementation checklist

* [ ] Create a canonical research definition table.

Example:

```cpp
enum class ResearchId {
    IronWeapons,
    Crossbows,
    Pikes,
    Counterweight,
    PlateHelm
};

struct ResearchDef {
    ResearchId id;
    int bit;
    int costGold;
    int costWood;
    int ticks;
    EntityType requiredBuilding;
    std::optional<EntityType> requiredOwnedBuilding;
};
```

* [ ] Add lookup:

```cpp
const ResearchDef* researchDef(ResearchId id);
const ResearchDef* researchDefFromBit(int bit);
```

* [ ] Add validation:

```cpp
CanResearchResult canResearch(
    const Game& game,
    PlayerId player,
    const Entity& building,
    ResearchId id
);
```

* [ ] Add execution:

```cpp
CommandResult startResearch(
    Game& game,
    PlayerId player,
    EntityId buildingId,
    ResearchId id
);
```

* [ ] Update player research input to dispatch a `Research` command.
* [ ] Update AI research to dispatch the same `Research` command.
* [ ] Remove direct AI writes to `researching`, `researchProgress`, and `researchTime`.
* [ ] Ensure the same costs and durations are used for human and AI unless an explicit difficulty modifier is added.
* [ ] If difficulty modifiers are desired, apply them through a named `AITuning` modifier, not hardcoded AI research times.

## Tests

* [ ] Human research subtracts correct resources.
* [ ] AI research subtracts correct resources.
* [ ] Human and AI use the same base research duration.
* [ ] Research cannot start twice.
* [ ] Already researched technology is rejected.
* [ ] Counterweight requires Castle if that is intended.
* [ ] Blacksmith must be complete.
* [ ] Research completion sets the correct player research bit.

## Done when

* [ ] There is one research definition source.
* [ ] There is one research execution path.
* [ ] AI no longer mutates research fields directly.
* [ ] Player input no longer contains inline research cost/timing logic.

---

# Phase 2 — Complete the command-system migration

This is the highest-value architectural phase.

The code already has a `CommandType` enum with many entries, but the dispatcher only implements a subset. Meanwhile, input and AI still mutate game state directly in multiple places. The goal is to make commands the only way gameplay actions are applied. 

---

## 2.1 Replace loosely packed command fields with typed payloads

## Current problem

`Command` has generic fields:

```cpp
Selection selection;
MapPos targetTile;
int targetEntity;
EntityType entityType;
int groupIndex;
```

These fields mean different things depending on command type. `groupIndex` is currently used as both a control group index and coordinate packing shim. 

## Required change

Replace overloaded command fields with typed payloads.

## Recommended design

```cpp
using EntityId = int;
using PlayerId = int;

struct Selection {
    EntityId primaryId = -1;
    std::vector<EntityId> ids;
};

struct MoveCommand {
    Selection selection;
    MapPos target;
};

struct AttackCommand {
    Selection selection;
    EntityId targetId;
};

struct AttackMoveCommand {
    Selection selection;
    MapPos target;
};

struct GatherCommand {
    Selection selection;
    MapPos resourceTile;
};

struct BuildCommand {
    EntityId builderId;
    EntityType buildingType;
    MapPos tile;
};

struct BuildLineCommand {
    EntityId builderId;
    EntityType buildingType;
    MapPos start;
    MapPos end;
};

struct TrainCommand {
    EntityId buildingId;
    EntityType unitType;
};

struct ResearchCommand {
    EntityId buildingId;
    ResearchId researchId;
};

struct GarrisonCommand {
    Selection selection;
    EntityId targetBuildingId;
};

struct EjectGarrisonCommand {
    EntityId containerId;
};

struct SetRallyCommand {
    EntityId buildingId;
    MapPos rallyTile;
};

struct HoldPositionCommand {
    Selection selection;
};

struct StopCommand {
    Selection selection;
};

struct SelectCommand {
    MapPos tile;
};

struct BoxSelectCommand {
    MapPos start;
    MapPos end;
};

struct SelectAllOfTypeInViewCommand {
    MapPos tile;
    Rect viewRect;
};

struct AssignControlGroupCommand {
    int groupIndex;
    Selection selection;
};

struct RecallControlGroupCommand {
    int groupIndex;
};

struct TogglePauseCommand {};
struct SaveCommand { std::string path; };
struct LoadCommand { std::string path; };
struct ResignCommand {};
```

Then:

```cpp
using CommandPayload = std::variant<
    MoveCommand,
    AttackCommand,
    AttackMoveCommand,
    GatherCommand,
    BuildCommand,
    BuildLineCommand,
    TrainCommand,
    ResearchCommand,
    GarrisonCommand,
    EjectGarrisonCommand,
    SetRallyCommand,
    HoldPositionCommand,
    StopCommand,
    SelectCommand,
    BoxSelectCommand,
    SelectAllOfTypeInViewCommand,
    AssignControlGroupCommand,
    RecallControlGroupCommand,
    TogglePauseCommand,
    SaveCommand,
    LoadCommand,
    ResignCommand
>;

struct Command {
    PlayerId issuer = 0;
    CommandPayload payload;
};
```

## Implementation checklist

* [ ] Introduce typed command payload structs.
* [ ] Keep old `CommandType` temporarily only if needed for migration.
* [ ] Add conversion helpers from old command shape to new payloads.
* [ ] Remove coordinate packing from `groupIndex`.
* [ ] Remove ambiguous uses of `targetTile`, `targetEntity`, and `entityType` where a typed payload is clearer.
* [ ] Make `dispatchCommand()` exhaustive.
* [ ] Add compile-time enforcement:

  * [ ] `std::visit` over variant, or
  * [ ] `-Wswitch-enum` if enum remains.

## Done when

* [ ] No command payload requires decoding bit-packed coordinates.
* [ ] Every command has a clear typed payload.
* [ ] Adding a new command requires adding a handler explicitly.
* [ ] `groupIndex` no longer exists or only means control group index.

---

## 2.2 Make commands self-contained

## Current problem

Some command execution reads global state such as `g.selectedIds` instead of using the command’s own selection. This means the result can depend on what is selected at execution time rather than what was selected when the command was created. 

## Required change

Commands must contain everything needed to execute deterministically.

## Implementation checklist

* [ ] Update group operations to take explicit selection:

```cpp
void orderGroupMove(Game& game, const Selection& selection, MapPos target);
void orderGroupAttack(Game& game, const Selection& selection, EntityId targetId);
void orderGroupAttackMove(Game& game, const Selection& selection, MapPos target);
```

* [ ] Update `cmdAtTileGroup()`:

```cpp
void cmdAtTileGroup(Game& game, const Selection& selection, MapPos target);
```

* [ ] Update `cmdAtTileSingle()`:

```cpp
void cmdAtTileSingle(Game& game, EntityId selectedId, MapPos target);
```

* [ ] Remove direct reads from:

  * [ ] `g.selectedIds`
  * [ ] `g.selectedId`
  * [ ] `view.cursorX`
  * [ ] `view.cursorY`

inside command execution.

* [ ] Selection may still be mutated by explicit selection commands.
* [ ] Game mode may still be mutated by input/UI handling, not by simulation services.

## Tests

* [ ] Create a command with selection A.
* [ ] Change global selection to B before dispatch.
* [ ] Dispatch command.
* [ ] Assert selection A receives the order, not B.

## Done when

* [ ] Commands execute based only on command data plus current game state.
* [ ] Command execution does not depend on the current UI selection unless the command is a selection command.
* [ ] Tests prove selection drift cannot affect command execution.

---

## 2.3 Implement or remove every `CommandType`

## Current problem

The enum lists many commands, but the dispatcher only handles a subset. This makes the architecture appear more migrated than it is. 

## Required change

Every command type must either be fully implemented or removed.

## Implementation checklist

Implement dispatcher support for:

* [ ] `Move`
* [ ] `Attack`
* [ ] `AttackMove`
* [ ] `Gather`
* [ ] `Build`
* [ ] `BuildLine`
* [ ] `Train`
* [ ] `Research`
* [ ] `Garrison`
* [ ] `EjectGarrison`
* [ ] `SetRally`
* [ ] `HoldPosition`
* [ ] `Stop`
* [ ] `Select`
* [ ] `BoxSelect`
* [ ] `SelectAllOfTypeInView`
* [ ] `AssignControlGroup`
* [ ] `RecallControlGroup`
* [ ] `TogglePause`
* [ ] `Save`
* [ ] `Load`
* [ ] `Resign`
* [ ] `MarketTrade`
* [ ] `ToggleGate`
* [ ] `ToggleTrebuchetPacked`

Optional/debug commands:

* [ ] `ToggleDiagnostics`
* [ ] `RevealMapDebug`
* [ ] `CycleIdleWorker`
* [ ] `CycleUnit`
* [ ] `JumpToHomeBase`

## Done when

* [ ] Dispatcher has no silent default for known commands.
* [ ] Every command has a test or is explicitly documented as UI-only.
* [ ] Input no longer directly applies gameplay actions.

---

## 2.4 Split input handling from command execution

## Current problem

`input_controller.cpp` mixes terminal key handling, mouse handling, command resolution, game mutation, save/load, market trades, debug reveal, selection, wall drag, rally mode, attack-move mode, research, and training. 

## Required change

Input should produce high-level intents. Commands should mutate game state.

## Recommended new files

```text
src/input/input_intent.h
src/input/input_mode_controller.h
src/input/input_mode_controller.cpp
src/input/terminal_input_adapter.cpp
src/input/mouse_input_adapter.cpp

src/commands/command.h
src/commands/command_dispatcher.cpp
src/commands/command_validation.cpp
src/commands/context_resolver.cpp

src/ui/selection_controller.cpp
src/ui/control_group_controller.cpp
```

## Recommended intent shape

```cpp
enum class InputIntentType {
    MoveCursor,
    FastMoveCursor,
    SelectAtCursor,
    ContextCommandAtCursor,
    BeginBuildMenu,
    ChooseBuildItem,
    BeginTrainMenu,
    ChooseTrainItem,
    BeginResearchMenu,
    ChooseResearchItem,
    BeginRallySet,
    ConfirmRally,
    BeginAttackMove,
    ConfirmAttackMove,
    BeginWallDrag,
    UpdateWallDrag,
    ConfirmWallDrag,
    CancelMode,
    SaveSlot,
    LoadSlot,
    ToggleHelp,
    ToggleDiagnostics,
    TogglePause,
    Resign
};

struct InputIntent {
    InputIntentType type;
    std::optional<MapPos> tile;
    std::optional<EntityType> entityType;
    std::optional<ResearchId> researchId;
    std::optional<int> slot;
};
```

## Implementation checklist

* [ ] Keep `handleInput(int ch)` initially, but make it delegate.
* [ ] Extract cursor movement to a cursor/input mode controller.
* [ ] Extract build menu choice handling.
* [ ] Extract train menu choice handling.
* [ ] Extract research menu choice handling.
* [ ] Extract rally selection handling.
* [ ] Extract wall drag handling.
* [ ] Extract attack-move target handling.
* [ ] Extract mouse map coordinate conversion.
* [ ] Extract minimap click handling.
* [ ] Convert gameplay actions to commands.
* [ ] Leave only UI state transitions inside input code.

## Forbidden after this phase

The following should not appear in input adapters:

```text
orderBuild(
orderTrain(
orderMove(
orderAttack(
orderGather(
spawnEntity(
ejectGarrison(
saveGame(
loadGame(
g.players[...].gold -=
g.players[...].wood -=
g.players[...].food -=
```

Save/load can be triggered by input, but through `SaveCommand` / `LoadCommand`.

## Done when

* [ ] Input files do not call gameplay order functions directly.
* [ ] Input files do not spend resources directly.
* [ ] Input files do not spawn entities directly.
* [ ] Input files produce intents or commands.
* [ ] Existing terminal controls still work.

---

# Phase 3 — Centralize rules into domain services and definitions

This phase removes duplicated costs, timings, rules, and side effects.

---

## 3.1 Centralize unit/building costs and production rules

## Current problem

`STATS` contains many unit/building values, but `orderTrain()` still has a manual food-cost switch. Production permissions are also hardcoded in `orderTrain()`. 

## Required change

All production rules should live in one canonical definition layer.

## Recommended data

```cpp
struct ResourceCost {
    int gold = 0;
    int wood = 0;
    int food = 0;
};

struct EntityStats {
    const char* name;
    char glyph;
    int maxHp;
    int attack;
    int range;
    int armor;
    int speed;
    ResourceCost cost;
    int trainTime;
    int sizeW;
    int sizeH;
    int supplyProvided;
    int supplyUsed;
    bool isBuilding;
    uint32_t traits;
};

struct ProductionRule {
    EntityType producer;
    std::vector<EntityType> allowedUnits;
    int queueLimit;
};
```

## Implementation checklist

* [ ] Add food cost to `EntityStats` or a parallel canonical `UnitDef`.
* [ ] Remove manual food-cost switch from `orderTrain()`.
* [ ] Add production rules table:

  * [ ] Town Hall → Peasant.
  * [ ] Barracks → Militia, Archer, Spearman, Catapult, Ram.
  * [ ] Stable → Knight.
  * [ ] Castle → Peasant, Trebuchet.
  * [ ] Dock → Fishing Boat, Warship, Transport.
* [ ] Add `canTrain()`:

```cpp
CanTrainResult canTrain(
    const Game& game,
    PlayerId player,
    const Entity& producer,
    EntityType unitType
);
```

* [ ] Add `startTraining()`:

```cpp
CommandResult startTraining(
    Game& game,
    PlayerId player,
    EntityId producerId,
    EntityType unitType
);
```

* [ ] Make `orderTrain()` a compatibility wrapper that calls `startTraining()`.
* [ ] Update command dispatcher and AI to use `startTraining()` through command dispatch.

## Tests

* [ ] Each producer accepts only intended unit types.
* [ ] Food, gold, wood, supply, queue limit, and construction-state validation are enforced.
* [ ] Queue behavior matches existing behavior.
* [ ] No player resource can become negative.
* [ ] Training time comes from canonical definition.

## Done when

* [ ] There is no manual food-cost switch.
* [ ] Production permissions are table-driven.
* [ ] Human and AI training use the same code path.

---

## 3.2 Centralize build rules

## Required change

Building placement and cost spending should use one build service.

## Recommended API

```cpp
CanBuildResult canBuild(
    const Game& game,
    const WorldIndex& world,
    PlayerId player,
    EntityId builderId,
    EntityType buildingType,
    MapPos tile
);

CommandResult startBuild(
    Game& game,
    WorldIndex& world,
    PlayerId player,
    EntityId builderId,
    EntityType buildingType,
    MapPos tile
);

CommandResult startBuildLine(
    Game& game,
    WorldIndex& world,
    PlayerId player,
    EntityId builderId,
    EntityType buildingType,
    MapPos start,
    MapPos end
);
```

## Implementation checklist

* [ ] Move cost checks out of `orderBuild()` into build service.
* [ ] Move placement checks into build service.
* [ ] Make `orderBuild()` a compatibility wrapper.
* [ ] Ensure farms, docks, gates, walls, and normal buildings use the same validation style.
* [ ] Ensure build service knows builder owner.
* [ ] Emit events/status through an event sink, not directly from service.

## Tests

* [ ] Peasant can build valid building.
* [ ] Non-builder cannot build.
* [ ] Invalid terrain rejected.
* [ ] Insufficient resources rejected.
* [ ] Dock requires shoreline.
* [ ] Farm cannot be built in winter.
* [ ] Wall/gate use canonical costs.
* [ ] AI and human build through same service.

## Done when

* [ ] All build paths call the build service.
* [ ] No build path directly spends resources outside the service.
* [ ] No build path directly spawns buildings outside the service except the service itself.

---

## 3.3 Centralize research rules

This continues the Phase 1 research fix and turns it into final architecture.

## Implementation checklist

* [ ] Move research definitions into:

```text
src/domain/research_defs.h
src/domain/research_defs.cpp
src/domain/research_service.h
src/domain/research_service.cpp
```

* [ ] Make player input, AI, save/load, render status, and building visual state consume canonical research IDs.
* [ ] Replace raw research bit handling at boundaries where possible.
* [ ] Keep the bitmask internally if it is useful for save compatibility.
* [ ] Add conversion functions:

```cpp
int researchBit(ResearchId id);
ResearchId researchIdFromBit(int bit);
```

## Done when

* [ ] Research cost, duration, required building, and completion effect are defined once.
* [ ] AI does not contain direct research timings.
* [ ] Input does not contain direct research timings.
* [ ] Save/load can still persist research safely.

---

## 3.4 Centralize market trades

## Current problem

Market trade mutates `Player` resources directly inside input handling. 

## Required change

Market trade should be a domain command.

## Implementation checklist

* [ ] Add:

```cpp
enum class MarketTradeType {
    GoldToWood,
    WoodToGold,
    GoldToFood,
    FoodToGold
};

struct MarketTradeCommand {
    EntityId marketId;
    MarketTradeType type;
};
```

* [ ] Add `MarketTradeDef` table:

```cpp
struct MarketTradeDef {
    CargoResource from;
    int fromAmount;
    CargoResource to;
    int toAmount;
};
```

* [ ] Add `canTrade()` and `executeTrade()`.
* [ ] Input should only choose trade type and dispatch command.

## Tests

* [ ] Valid trade subtracts and adds correct resources.
* [ ] Insufficient resource rejected.
* [ ] Cannot trade without selected complete market.
* [ ] Market owned by another player rejected.

## Done when

* [ ] Input no longer directly mutates market resources.
* [ ] Market rates are defined in one table.

---

# Phase 4 — Replace direct UI side effects with game events

## Current problem

Domain/order code calls UI functions such as `setStatus()` and `addActionMarker()` directly. Examples appear in movement, attack, build, gather, training, selection, and command resolution paths. 

## Required change

Simulation/domain logic should emit events. UI decides how to display them.

## Recommended design

```cpp
enum class GameEventType {
    StatusMessage,
    ActionMarker,
    ResourcesChanged,
    EntitySpawned,
    EntityDestroyed,
    CommandRejected,
    ResearchStarted,
    ResearchCompleted,
    TrainingStarted,
    TrainingQueued,
    BuildingPlaced
};

struct GameEvent {
    GameEventType type;
    PlayerId player = -1;
    EntityId entityId = -1;
    MapPos tile{-1, -1};
    std::string message;
    char markerGlyph = 0;
};

class EventSink {
public:
    virtual void emit(const GameEvent& event) = 0;
};
```

Short-term adapter:

```cpp
class LegacyUiEventSink : public EventSink {
public:
    void emit(const GameEvent& event) override {
        // writes g.statusMsg, g.statusTimer, g.actionMarkers
    }
};
```

## Implementation checklist

* [ ] Add `EventSink`.
* [ ] Add `LegacyUiEventSink`.
* [ ] Thread `EventSink&` through command dispatch and domain services.
* [ ] Replace domain calls to `setStatus()` with events.
* [ ] Replace domain calls to `addActionMarker()` with events.
* [ ] Keep `setStatus()` as UI compatibility only.
* [ ] Do not emit human-only messages from domain logic using `owner == 0`.
* [ ] Let UI filter events by local player.

## Done when

* [ ] Domain services do not call `setStatus()` directly.
* [ ] Domain services do not call `addActionMarker()` directly.
* [ ] Human/AI differences are handled by event filtering, not hardcoded in simulation.
* [ ] Existing status messages and action markers still appear in-game.

---

# Phase 5 — Introduce `GameContext`, typed IDs, and real module headers

---

## 5.1 Introduce context objects

## Current problem

Most systems read or mutate global `Game g`. This blocks testing, replay, deterministic simulation, and multi-game instances. 

## Required change

New code should receive explicit context.

## Recommended types

```cpp
struct GameContext {
    Game& game;
    WorldIndex& world;
    EventSink& events;
    Rng& rng;
};

struct UiContext {
    ViewState& view;
    InputState& input;
    EventSink& events;
};
```

## Implementation checklist

* [ ] Add `GameContext`.
* [ ] Add `UiContext`.
* [ ] Convert command dispatcher to accept `GameContext&`.
* [ ] Convert domain services to accept `GameContext&` or explicit `Game&`.
* [ ] Keep global `g` only:

  * [ ] in app/platform entry points,
  * [ ] in temporary legacy wrappers,
  * [ ] where migration has not yet reached.
* [ ] Add a comment or document rule:

```text
New simulation/domain code must not directly use global g.
```

## Done when

* [ ] New command/domain/AI code can be tested with a local `Game` instance.
* [ ] Global `g` is no longer required by newly written services.
* [ ] Existing game still runs through compatibility wrappers.

---

## 5.2 Introduce typed IDs and safer position types

## Required change

Raw `int` IDs and sentinel coordinate pairs should be made safer.

## Implementation checklist

Start simple:

```cpp
using EntityId = int;
using PlayerId = int;
```

Then migrate toward stronger types later if desired.

* [ ] Replace raw entity ID parameters with `EntityId`.
* [ ] Replace raw owner parameters with `PlayerId`.
* [ ] Use `MapPos` consistently.
* [ ] Replace `{-1, -1}` sentinel positions with `std::optional<MapPos>` in new code.
* [ ] Keep save/load compatibility with raw integers.

## Done when

* [ ] New APIs clearly distinguish entity IDs, player IDs, and coordinates.
* [ ] New APIs avoid overloaded `int` parameters where practical.

---

## 5.3 Replace empty wrapper headers with real module headers

## Current problem

Several headers only contain:

```cpp
#pragma once
#include "realm.h"
```

Examples include core headers for entity definitions, entity queries, game state, RNG, terrain definitions, types, and validation. These are transitional shims. 

## Required change

Headers should declare the module they represent.

## Target layout

```text
src/core/types.h
src/core/game_state.h
src/core/entity_defs.h
src/core/entity_query.h
src/core/terrain_defs.h
src/core/rng.h
src/core/validation.h

src/domain/build_service.h
src/domain/production_service.h
src/domain/research_service.h
src/domain/resource_service.h
src/domain/garrison_service.h
src/domain/combat_service.h

src/commands/command.h
src/commands/command_dispatcher.h
src/commands/command_validation.h
src/commands/context_resolver.h
```

## Implementation checklist

* [ ] Move type declarations out of `realm.h` into `core/types.h`.
* [ ] Move `Game`, `Entity`, `Player`, `Tile` declarations into `game_state.h`.
* [ ] Move `STATS`, `entityDef()`, visual state helpers into `entity_defs.h`.
* [ ] Move terrain definitions into `terrain_defs.h`.
* [ ] Move query declarations into `entity_query.h`.
* [ ] Move validation declarations into `validation.h`.
* [ ] Turn `realm.h` into a compatibility umbrella header.
* [ ] Gradually replace `#include "realm.h"` in `.cpp` files with narrower includes.

## Done when

* [ ] Empty wrapper headers no longer exist.
* [ ] `realm.h` is optional for most modules.
* [ ] Include dependencies are clearer and smaller.
* [ ] The project still builds.

---

# Phase 6 — Add `WorldIndex` and replace repeated full scans

## Current problem

Entity lookup and tile occupancy are mostly linear scans. AI and command logic repeatedly iterate all entities and sometimes all map tiles. `findEntity()`, `entityAt()`, and AI gathering/build placement are examples. 

## Goal

Add a query/index layer that provides fast, consistent access to entities, ownership, occupancy, nearby objects, and resources.

---

## 6.1 Build the initial `WorldIndex`

## Recommended design

Avoid storing long-lived raw pointers because `g.entities` can reallocate when entities are spawned. Prefer entity indices or IDs.

```cpp
struct WorldIndex {
    std::unordered_map<EntityId, size_t> entityIndexById;

    std::array<std::vector<EntityId>, MAX_PLAYERS> entitiesByOwner;
    std::array<std::vector<EntityId>, MAX_PLAYERS> unitsByOwner;
    std::array<std::vector<EntityId>, MAX_PLAYERS> buildingsByOwner;

    std::unordered_map<int, std::vector<EntityId>> entitiesByTile;

    OccupancyGrid unitOccupancy;
    OccupancyGrid buildingOccupancy;

    ResourceIndex resources;
};
```

Tile key:

```cpp
inline int tileKey(int x, int y) {
    return y * MAP_W + x;
}
```

Resource index:

```cpp
struct ResourceTile {
    MapPos pos;
    CargoResource type;
    int amount;
};

struct ResourceIndex {
    std::vector<ResourceTile> wood;
    std::vector<ResourceTile> gold;
    std::vector<ResourceTile> food;
    std::vector<ResourceTile> fish;
};
```

## Implementation checklist

* [ ] Add `WorldIndex`.
* [ ] Add:

```cpp
WorldIndex buildWorldIndex(const Game& game);
void rebuildWorldIndex(GameContext& ctx);
```

* [ ] Add query functions:

```cpp
Entity* entityById(Game& game, const WorldIndex& world, EntityId id);
const Entity* entityById(const Game& game, const WorldIndex& world, EntityId id);

std::vector<EntityId> entitiesAt(const WorldIndex& world, MapPos tile);
EntityId topEntityAt(const Game& game, const WorldIndex& world, MapPos tile);
EntityId topEntityAtOwner(const Game& game, const WorldIndex& world, MapPos tile, PlayerId owner);

bool isOccupied(const WorldIndex& world, MapPos tile, OccupancyLayer layer);
```

* [ ] Keep legacy `findEntity()` initially but implement it through `WorldIndex` where possible.
* [ ] Rebuild the index:

  * [ ] once at the start of a tick,
  * [ ] after entity spawn/destruction batches,
  * [ ] before AI planning,
  * [ ] before command validation if needed.

## Tests

* [ ] Index lookup matches old `findEntity()`.
* [ ] Tile lookup matches old `entityAt()`.
* [ ] Owner lookup returns the same live entities as a full scan.
* [ ] Occupancy grid matches old `buildOccupancyGrid()`.

## Done when

* [ ] `WorldIndex` exists and can be rebuilt.
* [ ] Query results match legacy scans.
* [ ] Tests cover index correctness.

---

## 6.2 Migrate hot query paths

## Implementation checklist

Migrate in this order:

* [ ] `findEntity()` internals.
* [ ] `entityAt()`.
* [ ] `entityAtOwner()`.
* [ ] `corpseAt()`.
* [ ] `buildOccupancyGrid()`.
* [ ] `canPlace()`.
* [ ] Group orders.
* [ ] Combat target lookup.
* [ ] AI counting.
* [ ] AI gathering.
* [ ] AI build placement.
* [ ] AI threat detection.

## AI-specific replacements

Replace:

```cpp
for every idle worker:
    scan entire map for nearest resource
```

with:

```cpp
for every idle worker:
    query ResourceIndex for candidate resource tiles
    choose nearest valid tile
```

Replace repeated AI count functions:

```cpp
aiCount()
aiCountAll()
aiBldg()
aiIdle()
```

with index-backed equivalents:

```cpp
aiView.countOwned(type)
aiView.firstOwnedBuilding(type)
aiView.idleWorkers()
```

## Done when

* [ ] AI gathering no longer scans the full map for every idle worker.
* [ ] AI counts do not repeatedly scan all entities.
* [ ] `canPlace()` does not rebuild occupancy inside tight placement loops.
* [ ] Performance counters show reduced scans.

---

# Phase 7 — Redesign AI around sensing, planning, and commands

## Current problem

AI currently mixes sensing, planning, and direct execution. It calls order functions directly, writes research fields directly, repeats full entity scans, unpacks broad `AIWorldView` fields in each subsystem, and suppresses unused variables with `(void)`. 

## Goal

AI should plan commands, then submit them to the same command system as the player.

---

## 7.1 Introduce `AIContext`

## Recommended design

```cpp
struct AITuning {
    int earlyGameEndTick;
    int midGameTick;
    int lateGameTick;

    int basePeasantCap;
    int maxPeasantCapLate;
    int attackGraceTicks;
    int attackThresholdEarly;
    int attackThresholdMid;
    int attackThresholdLate;
    int waveCooldownMid;
    int waveCooldownLate;

    int maxTowersLowThreat;
    int maxTowersHighThreat;

    std::vector<ResearchId> researchOrder;
};

struct AIContext {
    PlayerId owner;
    GameContext& game;
    const AIWorldView& view;
    const AITuning& tuning;
    std::vector<Command> plannedCommands;
};
```

## Implementation checklist

* [ ] Add `AITuning`.
* [ ] Move AI magic numbers into tuning:

  * [ ] peasant caps,
  * [ ] wave thresholds,
  * [ ] cooldowns,
  * [ ] tower caps,
  * [ ] forward aggression timing,
  * [ ] farm counts,
  * [ ] naval thresholds,
  * [ ] research order.
* [ ] Replace `Entity* playerTH` in `AIIntel` with safer data:

```cpp
std::optional<EntityId> playerTownCenterId;
std::optional<MapPos> playerTownCenterPos;
```

* [ ] Avoid storing raw pointers in AI views across mutation boundaries.

## Done when

* [ ] AI tuning numbers are centralized.
* [ ] AI views do not contain unsafe long-lived entity pointers.
* [ ] AI code no longer has repeated `(void)` suppression blocks from oversized interfaces.

---

## 7.2 Split AI into sensors, planners, and commandization

## Target layout

```text
src/ai/ai_sensors.cpp
src/ai/ai_economy_planner.cpp
src/ai/ai_production_planner.cpp
src/ai/ai_defense_planner.cpp
src/ai/ai_expansion_planner.cpp
src/ai/ai_naval_planner.cpp
src/ai/ai_combat_planner.cpp
src/ai/ai_command_executor.cpp
src/ai/ai_tuning.cpp
```

## New flow

```cpp
void tickAIForOwner(GameContext& ctx, PlayerId owner) {
    AIWorldView view = buildAIWorldView(ctx, owner);
    AIContext ai{owner, ctx, view, tuningFor(ctx.game.players[owner]), {}};

    planAIEconomy(ai);
    planAIProduction(ai);
    planAIDefense(ai);
    planAIFood(ai);
    planAINaval(ai);
    planAIExpansion(ai);
    planAICombat(ai);

    executeAICommands(ai);
}
```

## Implementation checklist

* [ ] Convert each `runAI*` function so it appends commands instead of directly calling `order*`.
* [ ] Add `executeAICommands()` that validates and dispatches commands.
* [ ] Keep the previous fixed order of AI passes initially to preserve behavior.
* [ ] Do not change AI strategy and architecture in the same commit unless unavoidable.
* [ ] Add debug logging for rejected AI commands.

## Commands AI should emit

* [ ] Build.
* [ ] Train.
* [ ] Research.
* [ ] Gather.
* [ ] Attack.
* [ ] AttackMove.
* [ ] Garrison.
* [ ] Eject/transport movement if applicable.
* [ ] Move.
* [ ] Help build/tend farm.

## Done when

* [ ] AI does not call `orderBuild()` directly.
* [ ] AI does not call `orderTrain()` directly.
* [ ] AI does not call `orderAttack()` directly except through command execution.
* [ ] AI does not write research fields directly.
* [ ] AI commands go through the same validation as human commands.

---

## 7.3 Refactor AI economy

## Current problem

`aiGather()` scans the full map for each idle gatherer. AI build worker selection also has special fallback behavior to avoid deadlocks. 

## Implementation checklist

* [ ] Use `ResourceIndex` for resource selection.
* [ ] Add resource priorities based on current shortages:

  * [ ] wood for early buildings/houses,
  * [ ] gold for army/research,
  * [ ] food for unit production,
  * [ ] fish only for fishing boats.
* [ ] Add worker assignment helper:

```cpp
std::optional<Command> planGatherForIdleWorker(AIContext& ai, EntityId workerId);
```

* [ ] Keep fallback behavior that can pull one worker off gathering for construction, but formalize it:

```cpp
EntityId chooseBuilder(AIContext& ai, BuilderPolicy policy);
```

## Done when

* [ ] Idle workers receive gather commands without map-wide scan per worker.
* [ ] Builder selection is centralized.
* [ ] AI no longer deadlocks at supply cap because all workers are gathering.
* [ ] Deterministic AI economy test passes.

---

## 7.4 Refactor AI production and research

## Implementation checklist

* [ ] Production planner should use:

  * [ ] `AIWorldView`,
  * [ ] `AITuning`,
  * [ ] `canTrain()`,
  * [ ] command generation.
* [ ] Research planner should use:

  * [ ] `ResearchDef`,
  * [ ] `canResearch()`,
  * [ ] command generation.
* [ ] Remove direct resource prechecks that duplicate service logic unless used only as cheap planning hints.
* [ ] Add “why rejected” debug information for AI command failures.

## Done when

* [ ] AI production and player production share rules.
* [ ] AI research and player research share rules.
* [ ] AI behavior remains deterministic for fixed seed.

---

## 7.5 Refactor AI combat

## Current problem

AI combat currently scans idle military, chooses attack waves, reacts to base threats, sends worker defense, controls trebuchets, and controls transports in one function. 

## Required change

Split combat into evaluators.

## Target components

```text
AIThreatEvaluator
AIArmyEvaluator
AITargetPicker
AISiegeController
AITransportController
AICombatPlanner
```

## Implementation checklist

* [ ] Extract idle army collection.
* [ ] Extract attack threshold calculation.
* [ ] Extract target scoring.
* [ ] Extract base threat detection.
* [ ] Extract worker-defense response.
* [ ] Extract trebuchet pack/deploy logic.
* [ ] Extract transport load/unload logic.
* [ ] Move all thresholds into `AITuning`.

## Done when

* [ ] `runAIAttackAndDefense()` is either removed or reduced to orchestration only.
* [ ] Combat helpers are individually testable.
* [ ] AI combat emits commands instead of direct order calls where practical.
* [ ] Existing combat behavior remains recognizably similar.

---

# Phase 8 — Save/load versioning and migration

## Current problem

Save/load is version-gated tightly. The earlier audit found the loader accepts only one save format version, which means every schema change risks breaking old saves.

## Goal

Make save/load explicitly versioned and migratable.

## Target layout

```text
src/save/save_schema.h
src/save/save_reader.cpp
src/save/save_writer.cpp
src/save/save_migrations.cpp
src/save/migrations/v7_to_v8.cpp
src/save/migrations/v8_to_v9.cpp
tests/fixtures/saves/
```

## Recommended design

```cpp
constexpr int CURRENT_SAVE_VERSION = 9;

struct SaveDoc {
    int version;
    // parsed raw/document representation
};

Expected<SaveDoc, SaveError> parseSaveFile(const std::string& path);
Expected<void, SaveError> migrateToCurrent(SaveDoc& doc);
Expected<void, SaveError> hydrateGame(const SaveDoc& doc, Game& game);
Expected<void, SaveError> writeSaveFile(const Game& game, const std::string& path);
```

## Implementation checklist

* [ ] Separate parsing from hydration.
* [ ] Separate hydration from validation.
* [ ] Add `CURRENT_SAVE_VERSION`.
* [ ] Add migration chain:

```cpp
while (doc.version < CURRENT_SAVE_VERSION) {
    migrateOneVersion(doc);
}
```

* [ ] Keep old save readers where necessary.
* [ ] Add clear error messages for unsupported versions.
* [ ] Validate after load.
* [ ] Validate before save in debug builds.
* [ ] Add golden fixtures:

  * [ ] basic current save,
  * [ ] save with garrison,
  * [ ] save with queued training,
  * [ ] save with research in progress,
  * [ ] save with wall/gate,
  * [ ] save with transport,
  * [ ] at least one older version.

## Done when

* [ ] Current saves round-trip.
* [ ] At least one older save version migrates successfully.
* [ ] Unsupported saves fail with a clear message.
* [ ] Schema changes require adding a migration or explicitly documenting why not.

---

# Phase 9 — Validation, recovery, and diagnostics

## Current problem

`validateGameState()` is valuable, but validation and recovery should be separated so bugs are not silently hidden during development. The uploaded code includes broad validation across selected IDs, control groups, entity IDs, targets, paths, cargo, projectiles, and more. 

## Goal

Make invalid state obvious in development and safely recoverable in release.

## Recommended design

```cpp
enum class ValidationSeverity {
    Warning,
    RecoverableError,
    FatalError
};

struct ValidationIssue {
    ValidationSeverity severity;
    std::string code;
    std::string message;
    std::optional<EntityId> entityId;
};

std::vector<ValidationIssue> validateGameStateDetailed(const Game& game);
RecoveryResult recoverGameState(Game& game, const std::vector<ValidationIssue>& issues);
```

## Implementation checklist

* [ ] Keep existing `validateGameState(std::string*)` as compatibility wrapper.
* [ ] Add detailed validation result type.
* [ ] Split validation from repair/recovery.
* [ ] Define which issues are recoverable:

  * [ ] stale selection IDs,
  * [ ] stale control group IDs,
  * [ ] stale target IDs,
  * [ ] invalid action markers.
* [ ] Define which issues are fatal:

  * [ ] entity outside map,
  * [ ] invalid entity type,
  * [ ] invalid owner,
  * [ ] negative HP/resource counters where not allowed,
  * [ ] path index outside path bounds.
* [ ] In debug builds:

  * [ ] validate pre-tick,
  * [ ] validate post-tick,
  * [ ] assert or fail loudly on fatal errors.
* [ ] In release builds:

  * [ ] validate periodically,
  * [ ] recover only known safe cases,
  * [ ] log compact issue summaries.

## Tests

* [ ] Corrupt selected ID and verify recovery.
* [ ] Corrupt control group and verify recovery.
* [ ] Corrupt entity position and verify fatal validation.
* [ ] Corrupt target ID and verify expected handling.
* [ ] Corrupt path index and verify fatal validation or repair, depending on chosen policy.

## Done when

* [ ] Validation does not silently hide development bugs.
* [ ] Recovery is explicit and limited.
* [ ] Validation issues are structured, not only strings.
* [ ] Tests cover both valid and invalid states.

---

# Phase 10 — Rendering and visual-definition cleanup

## Current problem

Terrain/entity visuals are split across several mappings and fallbacks. There is a canonical-looking `VisualTileParts` model, but renderer code still has parallel ASCII, SDL, fallback, and asset-detection logic. One shim identified earlier was a terrain image availability function that always returns false. 

## Goal

Create one renderer-neutral visual model consumed by all renderers.

## Target architecture

```text
TerrainDefinition
EntityDefinition
VisualTileParts
BuildingVisualState
EntityActionAnimationSpec
        ↓
RenderModel
        ↓
ASCII renderer
SDL symbolic renderer
SDL tileset renderer
Web renderer
```

## Recommended render descriptor

```cpp
struct RenderTile {
    MapPos tile;
    GroundType ground;
    FeatureType feature;
    FeatureState featureState;
    std::vector<VisualDecalType> decals;
    bool explored;
    bool visible;
};

struct RenderEntity {
    EntityId id;
    EntityType type;
    MapPos tile;
    BuildingVisualState buildingState;
    TransportVisualState transportState;
    AnimalCarcassVisualState carcassState;
    const EntityActionAnimationSpec* animation;
    bool selected;
    bool visible;
};

struct RenderModel {
    std::vector<RenderTile> tiles;
    std::vector<RenderEntity> entities;
    std::vector<ActionMarker> actionMarkers;
};
```

## Implementation checklist

* [ ] Add `RenderModel`.
* [ ] Add a render model builder:

```cpp
RenderModel buildRenderModel(
    const Game& game,
    const WorldIndex& world,
    const ViewState& view,
    PlayerId localPlayer
);
```

* [ ] Make ASCII renderer consume `RenderModel`.
* [ ] Make SDL renderer consume `RenderModel`.
* [ ] Make web renderer consume `RenderModel`, if applicable.
* [ ] Remove duplicate terrain visual switches where possible.
* [ ] Make `visualPartsForTerrain()` the canonical terrain visual source.
* [ ] Add asset-key helpers:

```cpp
std::string terrainAssetKey(const VisualTileParts& parts);
std::string entityAssetKey(const RenderEntity& entity);
```

* [ ] Replace placeholder terrain image detection with manifest-based detection.
* [ ] Ensure missing asset logging uses canonical asset keys.

## Tests

* [ ] Each terrain type produces a valid `VisualTileParts`.
* [ ] Each terrain visual maps to an ASCII fallback.
* [ ] Each terrain visual maps to an asset key.
* [ ] Each entity visual state maps to a render descriptor.
* [ ] Missing assets are logged once per key, not repeatedly.

## Done when

* [ ] Terrain visuals are defined once.
* [ ] Renderers do not maintain conflicting terrain/entity mappings.
* [ ] ASCII fallback still works.
* [ ] SDL/web rendering still works.
* [ ] Terrain asset detection is no longer a stub.

---

# Phase 11 — Map generation cleanup

The uploaded code includes map generation split between base generation and passes, with biome-specific terrain painting, resources, roads, water, ruins, and special continent/ocean generation. 

## Goal

Preserve generated-map feel while making generation more data-driven, testable, and invariant-checked.

## Implementation checklist

* [ ] Add `MapGenerationConfig`:

```cpp
struct MapGenerationConfig {
    unsigned seed;
    Biome biomeChoice;
    int goldClusterCount;
    int foodPatchCount;
    int ruinCount;
    int lakeCount;
    int roadCount;
};
```

* [ ] Pass config explicitly into map generation.
* [ ] Avoid direct reliance on global RNG in new mapgen code where practical.
* [ ] Use terrain definitions for resource/passability decisions where possible.
* [ ] Add map invariant checks:

  * [ ] no resources on invalid terrain,
  * [ ] `preWinterTerrain` set for every tile,
  * [ ] starting area is buildable/passable,
  * [ ] each player has reachable starting resources,
  * [ ] docks have possible shoreline positions on water maps.
* [ ] Add fixed-seed tests:

  * [ ] temperate map,
  * [ ] desert map,
  * [ ] snow map,
  * [ ] swamp map,
  * [ ] forest map,
  * [ ] volcanic map,
  * [ ] ocean/coastal map.

## Done when

* [ ] Mapgen is deterministic for a fixed seed.
* [ ] Mapgen invariants pass.
* [ ] Ocean/coastal maps remain playable.
* [ ] `preWinterTerrain` is consistently initialized.
* [ ] Mapgen code is split into named passes with clear inputs/outputs.

---

# Phase 12 — Gradually remove global mutable state from simulation

This phase should happen after commands, services, and WorldIndex exist.

## Goal

Make simulation testable without the global `g`.

## Implementation checklist

Migrate function families in this order:

* [ ] Entity query functions:

```cpp
findEntity
entityAt
entityAtOwner
corpseAt
findDepot
```

* [ ] Order/domain functions:

```cpp
orderMove
orderAttack
orderGather
orderBuild
orderTrain
orderGroupMove
orderGroupAttack
```

* [ ] Simulation tick functions.
* [ ] AI functions.
* [ ] Save/load functions.
* [ ] Render model builder.

## Migration pattern

Before:

```cpp
Entity* findEntity(int id);
void orderMove(Entity& e, int tx, int ty);
```

After:

```cpp
Entity* findEntity(Game& game, const WorldIndex& world, EntityId id);
CommandResult orderMove(GameContext& ctx, EntityId id, MapPos target);
```

Temporary wrapper:

```cpp
Entity* findEntity(int id) {
    return findEntity(g, legacyWorldIndex(), id);
}
```

## Done when

* [ ] Tests can create and mutate a local `Game`.
* [ ] Core simulation no longer requires the global `g`.
* [ ] Global `g` remains only in platform/app compatibility code.
* [ ] New code does not introduce new global `g` dependencies.

---

# Phase 13 — Stop using raw order functions as the public gameplay API

## Goal

After command and domain services are stable, raw `order*` functions should no longer be the external API used by input or AI.

## Implementation checklist

* [ ] Mark legacy order functions as internal or compatibility-only.
* [ ] Replace public calls with command dispatch.
* [ ] Add grep/static checks.

Forbidden outside command/domain internals:

```text
orderMove(
orderAttack(
orderGather(
orderBuild(
orderTrain(
orderGroupMove(
orderGroupAttack(
spawnEntity(
```

Allowed locations:

```text
src/domain/
src/commands/
src/sim/
tests/
legacy compatibility wrappers
```

## Done when

* [ ] Input does not call raw order functions.
* [ ] AI does not call raw order functions.
* [ ] Platform code does not call raw order functions.
* [ ] Only command/domain/simulation code applies gameplay mutations.

---

# Phase 14 — Documentation and enforcement

## Implementation checklist

* [ ] Add `docs/architecture.md`.
* [ ] Add `docs/commands.md`.
* [ ] Add `docs/ai.md`.
* [ ] Add `docs/save-format.md`.
* [ ] Add `docs/rendering.md`.
* [ ] Add `docs/testing.md`.

## `docs/architecture.md` should define

* [ ] What owns input.
* [ ] What owns command resolution.
* [ ] What owns command validation.
* [ ] What owns simulation mutation.
* [ ] What owns AI planning.
* [ ] What owns rendering.
* [ ] What owns save/load.
* [ ] Where global `g` is allowed.
* [ ] Where direct `setStatus()` is allowed.
* [ ] Where raw `order*` functions are allowed.

## Add static checks

Create a script such as:

```text
tools/check_architecture.sh
```

Checks:

* [ ] No `order*` calls in `src/input`.
* [ ] No direct resource mutation in `src/input`.
* [ ] No direct `smith.researching =` in `src/ai`.
* [ ] No `groupIndex` coordinate packing.
* [ ] No empty wrapper headers.
* [ ] No new `#include "realm.h"` in modules that have narrower headers.
* [ ] No direct `setStatus()` in domain services.
* [ ] No direct `addActionMarker()` in domain services.

## Done when

* [ ] Architecture rules are documented.
* [ ] CI or local checks enforce the most important rules.
* [ ] Future regressions are harder to introduce.

---

# Final acceptance checklist

The whole refactor is complete when all of the following are true.

## Commands/input

* [ ] Terminal input, mouse input, SDL input, web/mobile input if present all produce intents or commands.
* [ ] Commands have typed payloads.
* [ ] Commands do not rely on current global selection at execution time.
* [ ] Dispatcher handles every command explicitly.
* [ ] `X` hold-position conflict is fixed.
* [ ] Wall-line building uses typed payloads, canonical costs, and builder owner.
* [ ] Market trade is a command.
* [ ] Save/load are commands or explicit app-level actions routed through command handling.

## Domain rules

* [ ] Build rules are centralized.
* [ ] Train rules are centralized.
* [ ] Research rules are centralized.
* [ ] Market rules are centralized.
* [ ] Food costs are not hardcoded in `orderTrain()`.
* [ ] Research timings are not hardcoded separately in input and AI.
* [ ] No resource-spending path can make resources negative.

## AI

* [ ] AI uses `AIContext`.
* [ ] AI has centralized tuning.
* [ ] AI views do not store unsafe long-lived raw entity pointers.
* [ ] AI emits commands instead of directly mutating game state.
* [ ] AI research uses the shared research service.
* [ ] AI production uses the shared production service.
* [ ] AI gathering uses `ResourceIndex`.
* [ ] AI build placement uses shared placement/build validation.
* [ ] No repeated `(void)` blocks are needed to suppress unused AI view fields.

## World/query layer

* [ ] `WorldIndex` exists.
* [ ] Entity lookup is indexed.
* [ ] Tile occupancy is indexed.
* [ ] Resource tiles are indexed.
* [ ] AI hot paths no longer repeatedly scan all entities or all map tiles unnecessarily.
* [ ] Index correctness is tested against legacy scans.

## Save/load

* [ ] Save schema has a current version constant.
* [ ] Save parsing, migration, hydration, and validation are separate.
* [ ] Current saves round-trip.
* [ ] At least one older save version migrates.
* [ ] Unsupported saves fail clearly.
* [ ] Golden save fixtures exist.

## Rendering

* [ ] `RenderModel` exists.
* [ ] Terrain visuals come from `VisualTileParts`.
* [ ] Entity visual states come from canonical helpers.
* [ ] ASCII, SDL, and web renderers consume the same render model where applicable.
* [ ] Terrain image detection is not a hardcoded false stub.
* [ ] Missing asset logs use canonical asset keys.

## Validation

* [ ] Validation returns structured issues.
* [ ] Recovery is separate from validation.
* [ ] Debug builds fail loudly on fatal invalid state.
* [ ] Release builds recover only known safe cases.
* [ ] Corruption tests exist.

## Headers/global state

* [ ] Empty wrapper headers are gone.
* [ ] `realm.h` is no longer the primary include for every module.
* [ ] New code uses explicit `Game&`, `GameContext&`, or `UiContext&`.
* [ ] Global `g` is limited to platform/app compatibility boundaries.

## Tooling/docs

* [ ] Architecture docs exist.
* [ ] Command docs exist.
* [ ] AI docs exist.
* [ ] Save-format docs exist.
* [ ] Static architecture checks exist.
* [ ] Main test command runs all regression tests.

---

# Recommended commit order

Use small commits in this order:

1. Add baseline tests and deterministic test harness.
2. Fix `X` hold-position conflict.
3. Fix wall-line build ownership/cost/path.
4. Add research definitions and shared research service.
5. Move AI research to shared service.
6. Add typed command payloads.
7. Convert group commands to use explicit selections.
8. Complete command dispatcher coverage.
9. Split input into intents/mode handling.
10. Add production definitions and production service.
11. Add build service.
12. Add market service.
13. Add event sink and replace direct UI side effects in domain code.
14. Add `GameContext`.
15. Replace empty wrapper headers.
16. Add `WorldIndex`.
17. Migrate entity/tile/resource queries to `WorldIndex`.
18. Refactor AI sensors and context.
19. Refactor AI economy.
20. Refactor AI production/research.
21. Refactor AI combat.
22. Add save migration framework.
23. Add render model.
24. Consolidate terrain/entity visual mappings.
25. Clean up map generation and add map invariants.
26. Remove legacy wrappers that are no longer used.
27. Add architecture enforcement script.
28. Update docs.

This sequence keeps risk controlled: fix correctness bugs first, then complete the command boundary, then centralize rules, then optimize queries, then migrate AI and rendering.
