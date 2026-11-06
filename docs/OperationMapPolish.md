# Operation-map presentation — 2026-11-06

Scope: operation map only. Branch `feat/UI_Change1106` was clean before this work. No packaging, commit, push, save-format change, or gameplay/reward rule change.

## Rendering and timing

- Existing `/Game/Knight/UI/Menu/M_UI_StageMap` still draws the land texture, dot grid and authored irregular regions. The selection segment uses the same angular wobble equation as the material silhouette and `FindMarkerAt`; no rectangular selection mask or separate hit surface was added.
- A different region starts one **0.34 s EaseOutCubic** perimeter sweep. A smooth envelope briefly brightens the perimeter and segment, then reaches zero. The remaining blue outline is steady; the old region's selection weight fades over **0.12 s**. Selecting the same region, refreshing progression, dragging, zooming and restoring context do not replay it.
- Exposed `Knight|Map|Selection` values: `SweepDuration=.34`, `SelectionFadeDuration=.12`, `OutlineBrightness=.58`, `SelectionGlowWidth=7` Slate units, `InteriorOpacity=.13`.
- Right-panel content uses **0.18 s SmoothStep** fading: **0.06 s out**, swap all content while transparent, **0.12 s in**. The panel frame, position and dimensions remain stable. Rapid changes replace the pending destination; launch remains blocked throughout, and the final action is checked against both the displayed quest and the authoritative availability query.
- Connector, markers and quest selection use the blue menu palette. Operation-map buttons opt into a scoped static style; title/main-menu fills and orange maintenance styles are unchanged.
- Region and pin coordinates are unchanged. If a visible southern region's name/status block cannot fit below its silhouette, it is placed above that silhouette so it remains readable. Selecting a quest pin also brings its actual list row into view without animating the scroll.

## Data and navigation

`KnightOperationMapPresentation::Query` reads `UKnightQuestSettings::LoadCatalog()`, `UKnightQuestCatalog::CanSelectQuest`, and the active profile's `IsQuestCompleted`. It neither rolls nor grants rewards. Clear history and readiness are separate. Difficulty-specific history never implies that a whole region is complete.

Progression lock classification uses the existing reason's localization namespace/key (`QuestLocked`, `ExplorationLocked`, `StageLocked`). Disabled or unfinished combat content remains `출격 불가` with its original explanation, not a fabricated progression lock. Rewards, target, description and difficulty details remain available in the existing scroll area.

The catalog's actual map assignments are retained, even where the concept image differs:

| Region ID | Existing region name | Normal quest |
| --- | --- | --- |
| `Stage_0001` | 북부 폐허 도시 | `Quest_0001` / 잔열 |
| `Stage_0002` | 서부 해안 기지 | `Quest_0002` / 잔향 |
| `Region_Heukgwan` | 남부 지하 저장소 | `Quest_0003` / map exploration |

The existing disabled 흑관 normal/hard entries remain intact. `Quest_0003` still routes to `Stage_0003` and `L_Raid03`; it requires **잔향 normal (`Quest_0002`)** completion. The map displays exactly the existing three regions, without adding a separate `Stage_0003` region or inventing difficulty entries. Exploration says `맵 탐색 가능`, hides combat/reward details, and uses the existing map-test flow.

`UKnightOperationMapStateSubsystem` holds last region, quest/difficulty, center and zoom **in memory per profile slot**. It survives widget recreation/map travel in the same GameInstance, revalidates catalog membership on restoration, and clears the visible old-profile selection when profiles change. It does not add persistent fields or request saves. Esc's existing inspection-clearing and back behavior is preserved while remembering the last meaningful selection for reopening.

## Changed files and assets

- `Source/knight/Menu/UI/KnightMainMenuWidget.h/.cpp`: catalog indicators, profile/context lifecycle and guarded launch routing.
- `Source/knight/Menu/UI/KnightQuestMenuWidget.h/.cpp`: content fade, latest-target handling, readiness/history text, selected-row visibility and launch revalidation.
- `Source/knight/Menu/UI/KnightStageMapWidget.h/.cpp`: one-shot material parameters, previous-selection fade, blue marks/connector, status labels and view restoration.
- `Source/knight/Menu/UI/KnightOperationMapPresentation.h/.cpp` (new): read-only presentation query.
- `Source/knight/Menu/UI/KnightOperationMapStateSubsystem.h` (new): session/profile browsing context.
- `Source/knightUI/Widgets/knightButtonBase.h/.cpp`: opt-in static operation-map behavior; other screen behavior unchanged.
- `Tools/UI/Shaders/M_UI_StageMap.hlsl`, `Tools/UI/make_stage_map_material.py`: shader and reproducible graph update.
- `Tools/UI/make_operation_map_style.py` (new): scoped variants preserving existing button dimensions, fonts and sounds.
- `Content/Knight/UI/Menu/M_UI_StageMap.uasset`: updated existing effect material.
- `Content/Knight/UI/Style/BS_Knight_OperationMap.uasset`, `TS_Knight_OperationMapSelected.uasset` (new): static blue selection styles. Existing `/Game/Knight/UI` cook coverage already includes these; packaging settings were not changed.
- `Source/knightEditor/knightOperationMapCheckCommandlet.h/.cpp` (new): isolated integration/render checks.
- `Source/knightEditor/knightMaintenanceEntranceAssetsCommandlet.cpp`: private helper renamed `Save` → `SaveEntranceAsset` to resolve a pre-existing Unity-build name collision with the enhancement asset tool. No entrance behavior or asset changed.
- This report.

## Verification

Installed engine: **UE 5.8.4, CL 58902052**, association `5.8`. `knightEditor Win64 Development` build succeeded with `-WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4`.

Final operation-map commandlet: **140 checks, 0 failures**, **0 engine-reported errors/warnings**. It uses the actual menu Blueprint, menu GameMode initialization hook, runtime catalogs, progression/save APIs, Slate widgets, material and D3D RHI. A unique namespace and separate UserDir protect personal saves.

Covered: fresh profile locks; normal clear history alongside replay availability; unfinished hard entries distinguished from progression locks; Janhyang-normal unlock of exploration; no duplicate Raid03 region; correct launch delegate targets; launch rejection during fades; rapid region changes; same-region/refresh suppression; real map drag and wheel handlers; context restoration after reopening and widget recreation; profile A/B separation; unchanged inventory/money from browsing; static blue quest styles; actual shader map with zero compile errors.

Compiled `WBP_KnightMainMenu`, `WBP_KnightStageMap`, `WBP_KnightButton`, `BS_Knight_OperationMap`, and `TS_Knight_OperationMapSelected`: **0 errors, 0 warnings**. Two optional InputActionWidget binding notes are informational.

Rendered 25 consecutive selection frames plus representative ready, locked, cleared, exploration and reconstructed-menu states. Inspected 1920×1080, 1280×720 with an additional 1.25 UI-scale multiplier, and 2560×1440. Captures confirm blue silhouette feedback, fading content, unchanged panel bounds, readable southern-region labels, and scoped blue selection. These are **actual offscreen UI renders without the 3D hangar**, not full-scene or packaged captures.

Evidence root: `D:/Builds/Knight/OperationMap_20261106/`

- `build-verified.log`: final C++ build.
- `widgets-final.log`: widget/style compilation.
- `check-verified.log`: final 140-check run.
- `RenderVerified/SelectionSequence.mp4`: actual 60 FPS capture sequence with a one-second final-frame hold.
- `RenderVerified/SelectionSequence.png`: annotated contact sheet of rendered frames.
- `RenderVerified/*.png`: original RHI captures.
- `raid03-runtime.log`: existing end-to-end map-exploration regression, **13 of 14 assertions passed** (see limitation below).
- `raid03-audit.log`: read-only audit confirming the existing actors placed in `L_Raid03`.
- `preservation-check.json`: unchanged hashes for all 45 snapshotted personal saves and 9 quest/stage data assets.

### Existing Raid03 regression found during validation

The existing `knight.Raid03MapCheck` was also run through the real headless game travel flow with isolated profiles. It passed normal-Janhyang unlocking, disabled hard difficulty, `RequestStartQuest` acceptance, travel into `L_Raid03` in map-test mode without a reward run, prevention of completion/defeat settlement, return to `L_MainMenu`, unchanged money/items/quest progression, profile isolation, and save/reload of the unlock.

However, **the assertion "map exploration has no boss" failed**. A subsequent read-only editor audit confirmed two actors already placed in the map's persistent level:

- `/Game/Knight/Maps/L_Raid03.L_Raid03:PersistentLevel.BP_Janyeol_C_1`, class `/Game/Knight/Creatures/Janyeol/BP_Janyeol.BP_Janyeol_C`.
- `/Game/Knight/Maps/L_Raid03.L_Raid03:PersistentLevel.BP_Janhyang_C_1`, class `/Game/Knight/Creatures/Janhyang/BP_Janhyang.BP_Janhyang_C`.

The runtime log shows these actors acquiring the player and starting their engagement abilities. They are existing map content, not generated by the UI change. The map, catalogs, raid/GameFlow code and gameplay configuration remain unchanged; removing or disabling these actors would change the map-test behavior outside this UI-only task. Thus a boss-free Raid03 exploration session is **not** verified. The audit did not save the map. Its intentionally logged actor findings and existing navigation/GameplayCue warnings are separate from the clean 140-check operation-map run.

`git diff --check` and whitespace checks on the newly added text files passed. The final hash comparison found no changes to the snapshotted user saves or quest/stage data assets.

The first material-generator run logged transient missing-input warnings while adding the new graph connections. The final loaded material and compiled shader map passed validation; final rendering did not use the default-material fallback.

Computer Use was not used. Physical mouse/controller feel, live focus behavior in a full viewport, full hangar contrast, and packaged Shipping rendering remain unverified. Headless pointer-handler checks and offscreen renders are reported separately from these interactions.
