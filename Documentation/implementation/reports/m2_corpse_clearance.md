# M2: Frozen dead Cultist at the doorway

A lower Beretta aim from an earlier stopping point reproduced the user's intermittent
frozen-NPC report. The actor reached zero HP, but the new corpse box could not be
placed under the previous full-source-separation rule. Transfer failed and left the
old solid actor suspended. This is a second case beyond the earlier seat correction.

The reproduced cached anchor is approximately (-32,166.855,-1992), living half
extents (24.086,38.8541,24.086), and death half extents (40,4.91943,40). The living
body slightly straddles the doorway end at Z=-2016. Its full box only separates from
the two blocking faces along opposing X directions; the old search oscillated and
failed. Moving the corpse beyond the jambs is possible.

The alternative-axis search now admits source-center separation with an additional
sweep of the original living box through the correction. It retains the clear final
corpse box, unobstructed center path, original dimensions and all work/distance
bounds. The in-game Skeletons inspector records whether this extra sweep was
required. See [contracts](../../contracts/corpse_clearance_contracts.md).

No original game rule was rewritten or newly imported. The existing source selection
in [the corpse placement ledger](../sources/m2_corpse_placement_sources.json)
remains applicable: AI_Mgr::CreateCorpse supplies the raw dimensions, cached transform
and foot offset. This solver is explicitly replacement physics.

## Verification

Implementation commit: `e55601320386d604546367cca6402a69f33d0647`. Exact patch and changed-file hashes
are retained with the evidence.

Debug and inspector-free Release each pass **742/742 headless tests**,
including **121 private-data cases**. Each passes **7/7 GPU/window checks** on
**NVIDIA GeForce RTX 3060 Laptop GPU**, including the original mouse combat, the new
corpse traversal, skeleton presentation, lifecycle and NEW_LEVEL tests.

The focused Clang ASan/UBSan/leak selection covers **80 unique cases** (15 private).
The initial run passed 78, with one incorrect negative fixture and the expanded
combat test reaching its old 120-second timeout. The corrected fixture and full
combat run pass in the final three-case rerun (including the placement service).
No sanitizer diagnostic was observed. The new four-encounter test has a 240-second
time budget; no behavioral assertion was removed. The initial fixture accidentally
allowed a valid route above its short jamb, so the rejection fixture now has full-
height jambs. The solver itself was not changed to satisfy that assertion.

The exact combat regression also fails with the previous collision translation
unit, as recorded in the before-fix log. Five changed C++ files pass clang-format;
all local Markdown links resolve. The 10,923 reference files and six reference
repositories are unchanged. Full Debug/sanitizer recompilation exposes an existing
ItemPickups.cpp float/enum warning; that source is unchanged.

Commands, full logs and integrity results are in the [evidence](evidence/m2-corpse-clearance.json).

The new generated doorway fixture also adds a lintel rejection case: a short corpse
could fit but the swept living source would hit the lintel. Existing impossible-fit
and source-side floor protections remain tested. The private combat regression uses
normal spawn, the actual introductory cameras, keyboard movement/USE and synthetic
SDL mouse motion/primary fire. It does not teleport the player, alter an NPC, inject
damage or change flags/health/ammunition. It kills the authored Cultist with seven
Beretta shots, then kills the following Cultist with ten more shots, checks both
independent corpses and walks past each blockage at both 30 and 60 simulation updates
per second, with complete unload/reload checks. The later enemy uses ordinary
placement; the first needs the new swept-source branch. Normal weapon-drop pickups
can increase ammunition during the walk; tests never grant extra ammo.

The user reports later enemies too. The first two encounters are now covered, but this
correction does not establish that every reported position/death branch is fixed; fire/gib deaths and difficult fits
remain explicit limitations. M2 stays incomplete, and authored CityHub entry still
rejects 18 unsupported and one failed entry. Native Apple tests were not run here;
the separate macOS success report remains user-reported evidence.
