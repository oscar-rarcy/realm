Use case: stylized-concept
Asset type: coherent production batch source for a 2D RTS game tileset
Primary request: Generate exactly one square image containing a 2 by 2 batch sheet for the Realm peasant `idle` animation. This batch sheet is an intermediate consistency source; it will be split into standalone production `source.png` files before processing.

Shared character identity: one compact medieval male peasant/villager with the same rounded cartoon RTS proportions, blue helmet, blue tunic/player-colour cloth accents, tan skin, dark beard, brown belt, brown boots, thick clean outline, and readable small-sprite silhouette in every panel.

Consistency requirements: every panel must use the identical character design, palette, scale, camera height, lighting, outline weight, pixel-art/painted sprite finish, and foot baseline. All source art faces slightly toward screen right. Do not create left-facing source art; left facings are mirrored by the Realm renderer at runtime.

Panel order. Use these labels as instructions only; do not draw labels or text:
1. row 1, column 1: front frame 0, Relaxed idle, arms at sides, both feet planted., Realm front view: three-quarter front RTS sprite angle like the reference crop, body and face turned about 30-45 degrees toward screen right, one cheek and side of the helmet visible, not a straight-on symmetrical mascot pose
2. row 1, column 2: back frame 0, Relaxed idle, arms at sides, both feet planted., Realm back view: rear-right three-quarter RTS sprite angle matching the same diagonal rotation as the front pose, facing away toward screen right; the shoulders, belt, hem, and boots form a visible diagonal rather than a horizontal straight-back view. Show the back plus the screen-right side of the helmet, tunic, pouch, sleeve, and boot closer to the camera, with the far side partly hidden. Not a flat straight-on rear diagram.
3. row 2, column 1: front frame 1, Long-idle hold pose, arms crossed, both feet planted., Realm front view: three-quarter front RTS sprite angle like the reference crop, body and face turned about 30-45 degrees toward screen right, one cheek and side of the helmet visible, not a straight-on symmetrical mascot pose
4. row 2, column 2: back frame 1, Long-idle hold pose, arms crossed, both feet planted., Realm back view: rear-right three-quarter RTS sprite angle matching the same diagonal rotation as the front pose, facing away toward screen right; the shoulders, belt, hem, and boots form a visible diagonal rather than a horizontal straight-back view. Show the back plus the screen-right side of the helmet, tunic, pouch, sleeve, and boot closer to the camera, with the far side partly hidden. Not a flat straight-on rear diagram.

Layout: one square image, 2 columns by 2 rows, equal square panels, generous padding inside each panel, one complete centered sprite per panel, flat uniform #ff00ff magenta or clean transparent background. No gutters or labels are required, but the visual grid must be easy to split mechanically.

Positive references that must be viewed before generation:
- Positive reference to view before generation: C:\Users\Edward\Desktop\peasant 3.png

Reference hygiene: view only positive references that show the desired angle or the full source sheet. Do not view or attach wrong-angle crops, flat rear diagrams, face-on front crops, inconsistent failed batches, or other rejected images as generation references; they are for human diagnosis only and bias the pose.

Constraints: no text, no numbers, no watermark, no cropped reference-sheet art as the final sprite, no straight-on mascot front view, no flat rear diagram, no mirrored left-facing variants, no magenta inside the character or carried objects. Frame 1 of Peasant idle is the arms-crossed long-idle pose in both front and back panels.
