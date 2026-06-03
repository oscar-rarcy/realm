#pragma once

#include "commands/command.h"
#include "core/service_result.h"

// Transitional domain-order service for actions that still rely on legacy order
// helpers internally. The service owns issuer validation; callers should not
// mutate entity order state directly.

struct WorldIndex;

ServiceResult canMove(const Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, MapPos target);
ServiceResult startMove(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target);
ServiceResult startAttackMove(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target);

ServiceResult canAttack(const Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, EntityId targetId);
ServiceResult startAttack(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, EntityId targetId);

ServiceResult canGatherAt(Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, MapPos target);
ServiceResult startGather(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target);

ServiceResult canHelp(Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, EntityId targetId);
ServiceResult startHelp(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, EntityId targetId);

ServiceResult canGarrison(const Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, EntityId targetId);
ServiceResult startGarrison(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, EntityId targetId);
ServiceResult ejectGarrisonService(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection);

ServiceResult setRallyPoint(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target);
ServiceResult holdPosition(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection);
ServiceResult stopUnits(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection);
ServiceResult toggleGateMode(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection);
ServiceResult toggleTrebuchetPacked(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection);
