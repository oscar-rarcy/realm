# Note "docs\implementation\ascii-rts-hardening-plan.md" accepts all gpt-5.5's recommendations from this file

1. Should AI pull workers off gathering to build?

Decision needed.

Options:

Simple RTS behaviour: AI can pull any peasant from gathering when it needs a builder.
More deliberate economy: AI keeps some peasants permanently reserved for building.
Current behaviour: only idle peasants build, which causes deadlock.

Recommendation: let AI pull a gatherer when construction is important. That is the least brittle.

2. Should forests block movement/building?

Decision needed.

Current behaviour seems to allow units/buildings on forest-like terrain. That may or may not be intended.

Options:

RTS-like: forest blocks movement and building until chopped.
Lightweight ASCII style: forest is decorative/resource terrain and units can pass through.
Hybrid: passable, but not buildable.

Recommendation: make forests not buildable, but decide later whether they block movement. Blocking movement affects map generation and pathfinding more heavily.

3. Are berry bushes decorative or gatherable?

Decision needed.

Current map has T_BERRY, but peasants do not seem to gather from it.

Options:

Make berries gatherable food.
Rename/recolour them so they do not imply food.
Remove them for now.

Recommendation: make them gatherable. In an RTS, visible berry bushes naturally read as food.

4. Should town halls cost resources?

Decision needed.

Current town hall cost appears to be zero, but AI expansion can build town halls.

Options:

Town halls are free only for starting placement.
Town halls cost wood/gold/food when built.
AI cannot build town halls; only castles/outposts.
Expansion is intentionally free and fast.

Recommendation: starting town halls should be free via setup code, but buildable town halls should have a real cost.

5. How faction-coloured should enemies be?

Decision needed, but not urgent.

Options:

All enemies use one enemy colour.
Each AI owner has a different colour.
Use one colour on the main map, faction colours on minimap.

Recommendation: eventually use faction colours. For now, just fix owners 2 and 3 so they do not look like animals.

6. Should the game remain one big global-state prototype or start splitting systems?

Decision needed at project level.

Options:

Keep the current structure and only patch bugs.
Split AI, orders, economy, combat, and simulation into separate files.
Go further and introduce ECS-like architecture.

Recommendation: do not rewrite it into ECS. But do split entity.cpp soon, because it is already doing too much.

7. Should overloaded fields be cleaned up now?

Decision needed.

Examples:

gatherType also means gate lock.
carrying also means gate open/closed.
prodProgress means production or research.
rallyX/Y means rally or gather-return target.

Options:

Leave it until bugs appear.
Rename/comment the overloading.
Split into explicit fields now.

Recommendation: split the fields now. This is a small refactor compared with the cost of debugging weird future behaviour.