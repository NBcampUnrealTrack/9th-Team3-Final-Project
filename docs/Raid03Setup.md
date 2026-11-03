# Raid 03 setup

Raid 03 reuses the existing map and the same native `AknightRaidGameMode` base as Raid 01 and Raid 02. No monster or rewards from another raid are assigned.

## Asset wiring

| Asset | Configuration |
| --- | --- |
| `/Game/Knight/Maps/L_Raid03` | World Settings → GameMode Override = `BP_GM_Raid03` |
| `/Game/Knight/Blueprints/BP_GM_Raid03` | Parent = `/Script/knight.knightRaidGameMode`; DefaultRaidDefinition = `DA_RaidStage03` |
| `/Game/Knight/Raids/DA_RaidStage03` | TargetMonster = None; RequireTargetMonster = true; RequiredCompletedQuestId = `Quest_0002` (Janhyang Normal); TimeLimit = 0; Rewards empty |
| `/Game/Knight/Data/Stage/DA_StageCatalog` | `Stage_0003`: GameplayMap = `L_Raid03`; RaidDefinition = `DA_RaidStage03`; ShowOnMap = false; AllowMapExploration = true |
| `/Game/Knight/Data/Quests/DA_Quest_Raid03_Normal` | QuestId = `Quest_0003`; StageId = `Stage_0003`; Difficulty = Normal; ContentEnabled = true; MapExplorationOnly = true; prerequisite = `Quest_0002` |
| `/Game/Knight/Data/Quests/DA_QuestCatalog` | Existing `Region_Heukgwan` renamed `남부 지하 저장소`; `Quest_0003` is placed in that region at offset (+18, +6) |

BossId, BossDefinition and HardBossClass are unset. MoneyReward and FirstBossSocketSlots are zero; ClearRewards and PartBreakRewards are empty. The normal menu offers map exploration only after the selected profile has completed the Janhyang Normal quest (`Quest_0002`). No quest-run context is created for that travel. The region retains its original globe location (50, -40) and radius (14); the duplicate `Stage_0003` region marker is hidden. The original disabled Heukgwan Normal/Hard quest assets and their map placements remain intact for later content.

The existing `Stage_03` is a different developer test entry pointing to `L_FlowTest`; it is preserved. Existing quest IDs, Heukgwan placeholders, Raid 01/02 data and save formats are unchanged.

## Test the map now

After completing `Quest_0002` in a profile, select `남부 지하 저장소` on the operation map and choose `일반 · 탐색`. Use the `맵 탐색` button to enter `L_Raid03`; leaving/abandoning returns to the menu without a result. Before that completion, the placement remains locked. The existing disabled Heukgwan Normal/Hard choices are not enabled by this change.

For direct editor testing, open `/Game/Knight/Maps/L_Raid03` and use Play from its existing PlayerStart. No quest selection is necessary. The authored PlayerStart and all environment actor transforms are preserved. The pre-existing `BP_Janyeol_C_1` placeholder actor was removed from this map only; its Blueprint and monster assets remain unchanged.

Direct PIE/OpenLevel resolves the current map's unique catalog stage and clears stale active quest-run context. If the catalog cannot resolve a stage, the GameMode's existing `DefaultRaidDefinition` fallback still points to Raid 03. There is no `DefaultStageId` property in this project.

While a required target is unset, the GameMode enters exploration without a raid run, target binding, encounter clock or loading hold. Start, completion, defeat and debug-finish hooks cannot settle a result. Returning to the lobby does not record an abandonment. Player movement, combat controls, environmental geometry and the normal HUD are retained; this mode is not player invulnerability.

## When the monster is ready

1. Set `DA_RaidStage03.TargetMonster` to the actual monster definition. Leave `RequireTargetMonster` enabled. Set the intended `TimeLimit` and presentation data if needed.
2. Place the intended monster Blueprint in `L_Raid03`, with its monster definition matching `TargetMonster`, as Raid 01/02 do. The raid definition selects placed targets; it does not spawn a boss by itself. No placeholder spawn configuration was copied.
3. Set `DA_Quest_Raid03_Normal.BossDefinition` to that same definition and `BossId` to its authoritative Codex boss ID. Add/update the corresponding row in the Codex asset referenced by `DA_QuestCatalog.CodexData` so its BossId and MonsterDefinition agree.
4. Configure this quest's own `ClearRewards` (required by the current validator), any `PartBreakRewards` with real part tags, and intended money, socket and prerequisite values. Keep quest rewards in the quest definition; do not populate legacy raid Rewards to support this quest.
5. Turn off `DA_Quest_Raid03_Normal.MapExplorationOnly` after data validation. `ContentEnabled` is already true for exploration; leave it true for combat. The stage and map placement are already connected; no GameFlow code or ID changes are needed. The stage is not DevelopmentOnly. `Stage_0003.AllowMapExploration` may then be turned off once the target is present.

This change does not run packaging or alter cook settings. Add `L_Raid03` to the project's explicit MapsToCook list when it is approved for a future packaged release.

## Verification

Runtime changes are in `Source/knight/GameFlow/KnightGameFlowSubsystem.cpp`, `Source/knight/Raid/knightRaidDefinition.h`, and `Source/knight/Raid/knightRaidGameMode.{h,cpp}`. The regression helper is `Source/knightDev/Private/knightRaid03Check.cpp`; it is excluded from player Shipping builds.

Build and runtime evidence is recorded outside the repository at `D:\Builds\Knight\Raid03Setup_20261103`. `knight.Raid03Check` is an explicitly invoked editor-only check requiring an isolated `KnightSaveTestNamespace=Raid03Check...` and a matching isolated UserDir. It tests stale run context, map re-entry, blocked settlement, and original Raid 01/02 stage resolution.

- Installed engine: UE 5.8.4, CL 58902052; project association is `5.8`.
- `knightEditor Win64 Development` build succeeded (`build-final.log`).
- Fresh-process asset audit: 28/28 checks passed (`asset-verification.json` / `.log`). Raid 01/02/03 GameModes compiled; Raid 03 MapCheck reported 0 errors and 0 warnings. The commandlet completed with 0 errors and 4 existing project/asset warnings.
- `map-preservation.json` confirms only the placed Janyeol placeholder was removed, with all other actor transforms and the PlayerStart retained. An original map backup is outside the repository.
- The initial rendered-offscreen/NullRHI attempt stalled before executing the helper. The retry uses `-game -nullrhi -NoLoadingScreen -unattended -nosound`; the graphical loading screen is bypassed only in this test command line. The helper still checks the Raid GameState loading gate and pause state directly.
- Headless runtime retry exited successfully: `Raid03Check COMPLETE assertions=37 failures=0` (`runtime-final.log`). It loaded Raid 03 twice with stale Raid 02/Raid 01 quest contexts, checked its player and lack of bosses, blocked start/completion/defeat/debug settlement, and returned to the lobby without results. Raid 01 and Raid 02 each resolved their original definition, target and run stage. This is initial map/bootstrap and API behavior, not a live combat playthrough.
- The runtime log also contains startup errors from experimental editor toolset Python bindings and NullRHI cursor/CommonUI setup, before the helper runs. The 37 passing assertions do not claim an error-free rendered UI session; those startup systems were not changed here.
- All 45 pre-existing save files retained their SHA-256 hashes. No save format or ID migration changed. `git diff --check` passed.

Computer Use remains paused. Headless map-loading checks are distinct from interactive editor PIE, rendered navigation and live combat verification.

## Operation map follow-up (2026-11-03)

`Region_Heukgwan` is the authored lower map region. Its globe location (50, -40), radius (14), and existing disabled Heukgwan Normal/Hard placements remain unchanged. The region label is now `남부 지하 저장소`. `Stage_0003.ShowOnMap = false` removes the separate center marker, while `Quest_0003` is placed in `Region_Heukgwan` with a distinct (+18, +6) quest pin. `Stage_03` remains the unrelated developer test stage.

`DA_RaidStage03.RequiredCompletedQuestId = Quest_0002` uses the selected profile's completed Janhyang Normal quest. `Stage_0003.AllowMapExploration` permits its missing-monster map to launch after that unlock. `DA_Quest_Raid03_Normal` has `ContentEnabled = true`, `MapExplorationOnly = true`, and `Quest_0002` as its prerequisite. It remains a map visit without a boss, quest reward run, or quest completion. Heukgwan's separate Normal/Hard quests are still disabled.

The `knightEditor Win64 Development` build passed (`D:\Builds\Knight\Raid03MapFollowup_20261103\build-final.log`). A fresh-process asset audit passed 28/28 checks and compiled the Raid 01/02/03 GameMode Blueprints (`asset-verification.json`, `verify-assets.log`). The audit confirms the saved map and catalog wiring; it does not render the operation map. Headless runtime and visual behavior are tracked separately in this folder. The 45 existing user saves matched their pre-change SHA-256 hashes.

`knight.Raid03MapCheck` ran under an isolated save namespace and UserDir (`runtime.log`): 14/14 checks passed. It recorded a Janhyang Normal clear through the progression API, entered Raid 03 through `RequestStartQuest`, returned to the menu, confirmed no raid result/reward/quest completion, and verified a second profile stayed locked while the first profile's unlock survived slot reload. These are NullRHI code and map-loading checks. The map artwork, pin placement on screen, mouse/controller interaction, and rendered PIE flow were not visually verified.

The existing `knight.Raid03Check` ran again under another isolated profile (`runtime-regression.log`): 37/37 checks passed, including Raid 01/02 map loads with their original stage IDs, raid definitions, and targets. The fresh-profile Raid 03 lock assertions still pass. The helper's old assertion labels were updated to describe the profile unlock condition; the final editor build passed again after that edit.
