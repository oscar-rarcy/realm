# Peasant/Villager Reference Sheet Prompt

Use this when generating Realm peasant **reference/contact sheets**. These sheets are only used to choose a slot such as "top left" or "row 2 column 3" and to preserve visual style. They are not final production sources; final production frames are generated later as one standalone 1024x1024 sprite on one flat magenta background.

Generate four separate reference images when possible:

- `front_state_1`: facing the viewer, first frame for every action.
- `front_state_2`: facing the viewer, second frame for every action.
- `back_state_1`: facing away from the viewer, first frame for every action.
- `back_state_2`: facing away from the viewer, second frame for every action.

Each image must be a clear 4 by 4 contact sheet with consistent row and column spacing. A single solid magenta background across the whole sheet is fine; separate magenta squares and white gutters are not required for reference sheets. One complete sprite belongs in each grid slot. Keep the same camera, scale, lighting, outline thickness, outfit, helmet, beard, and blue player-colour cloth across all slots. Do not draw any transparency. Keep tools, limbs, baskets, logs, meat, wheat, or weapon arcs readable and mostly inside their visual slot, but this reference sheet does not need production-perfect crop bounds.

The character is a compact medieval villager/peasant in the same rounded cartoon RTS style as the supplied reference: blue helmet and tunic accents for player colour, brown leather boots and belt, tan face and hands, dark beard, thick clean outline, readable at small size.

## Exact 16 Squares

Read left to right, top to bottom:

1. `idle`: standing idle, relaxed arms at sides, both feet planted.
2. `walk`: walking with the front/near leg forward and the rear/far leg back.
3. `chop_wood`: chopping wood at the bottom of the axe swing, axe head low and forward.
4. `mine_gold`: mining at the bottom of the pickaxe swing, pickaxe head low and forward.
5. `gather_berries`: gathering berries with one hand reaching out toward a basket or bush-side basket.
6. `hoe_soil`: hoeing soil with arms outstretched and a long-handled farming hoe extended away from the body; the hoe has a small flat rectangular blade and is not an axe or pickaxe.
7. `gather_wheat`: gathering wheat using a sickle, sickle actively cutting wheat.
8. `build`: kneeling builder, hammer raised up.
9. `carry_wood`: carrying bundled logs while walking, front/near leg forward.
10. `carry_gold`: carrying a pile of stones/gold ore while walking, front/near leg forward.
11. `carry_berries`: carrying a basket of red berries while walking, front/near leg forward.
12. `carry_wheat`: carrying a bundle of wheat while walking, front/near leg forward.
13. `gather_meat`: gathering meat while holding a knife, knife actively cutting or reaching toward meat.
14. `carry_meat`: carrying a large cut of meat while walking, front/near leg forward.
15. `club_attack`: club attack with the club at the top of the swing, held overhead.
16. `death`: dead villager body lying on the ground, not a skeleton.

For `state_2`, keep the same square order but use these second frames:

1. `idle`: standing with arms crossed.
2. `walk`: walking with the rear/far leg forward and the front/near leg back.
3. `chop_wood`: axe at the top of the swing.
4. `mine_gold`: pickaxe at the top of the swing.
5. `gather_berries`: hand in basket with berries.
6. `hoe_soil`: arms pulled in after the hoe stroke, still holding the long-handled farming hoe with a small flat rectangular blade; not an axe or pickaxe.
7. `gather_wheat`: still holding the sickle while the free hand reaches for wheat.
8. `build`: kneeling builder, hammer down.
9. `carry_wood`: carrying bundled logs while walking, rear/far leg forward.
10. `carry_gold`: carrying stones/gold ore while walking, rear/far leg forward.
11. `carry_berries`: carrying berries while walking, rear/far leg forward.
12. `carry_wheat`: carrying wheat while walking, rear/far leg forward.
13. `gather_meat`: still holding the knife while taking meat with the free hand.
14. `carry_meat`: carrying meat while walking, rear/far leg forward.
15. `club_attack`: club at the bottom of the swing.
16. `death`: skeleton remains of the same villager in the same pose area, with clothing scraps visible enough to read as the same unit.

For back views, show the same 16 actions from behind. The viewer should see the back of the helmet, tunic, belt, pack/basket where applicable, and rear side of tools. Keep the state details identical: front leg versus rear leg, tool high versus low, hand out versus hand in basket, hammer up versus down, body versus skeleton.

## Negative Requirements

- Do not treat this reference sheet as final production art.
- Do not add labels, numbers, or text inside the image.
- Do not draw mirrored left/right variants; the game will mirror horizontally at runtime.
- Do not change player colour away from the single blue preview colour unless explicitly asked.
- Do not use magenta for berries, meat highlights, skin blush, or clothing trim.
