# Raid 03 setup

Raid 03 reuses the existing map and the same native `AknightRaidGameMode` base as Raid 01 and Raid 02. No monster or rewards from another raid are assigned.

## Asset wiring

| Asset | Configuration |
| --- | --- |
| `/Game/Knight/Maps/L_Raid03` | World Settings → GameMode Override = `BP_GM_Raid03` |
| `/Game/Knight/Blueprints/BP_GM_Raid03` | Parent = `/Script/knight.knightRaidGameMode`; DefaultRaidDefinition = `DA_RaidStage03` |
| `/Game/Knight/Raids/DA_RaidStage03` | TargetMonster = None; RequireTargetMonster = true; TimeLimit = 0; Rewards empty |
| `/Game/Knight/Data/Stage/DA_StageCatalog` | New `Stage_0003`: GameplayMap = `L_Raid03`; RaidDefinition = `DA_RaidStage03` |
| `/Game/Knight/Data/Quests/DA_Quest_Raid03_Normal` | QuestId = `Quest_0003`; StageId = `Stage_0003`; Difficulty = Normal; ContentEnabled = false |
| `/Game/Knight/Data/Quests/DA_QuestCatalog` | New quest reference and map placement `Quest_0003` → region `Stage_0003` |

BossId, BossDefinition and HardBossClass are unset. MoneyReward and FirstBossSocketSlots are zero; ClearRewards and PartBreakRewards are empty. The normal menu uses the existing disabled / 준비 중 state. Direct stage-launch requests also reject the missing required target. The generic map marker uses the stage's default (0, 0) position; adjust its presentation when the region design is finalized.

The existing `Stage_03` is a different developer test entry pointing to `L_FlowTest`; it is preserved. Existing quest IDs, Heukgwan placeholders, Raid 01/02 data and save formats are unchanged.

## Test the map now

Open `/Game/Knight/Maps/L_Raid03` in the editor and use Play from its existing PlayerStart. No quest selection is necessary. The authored PlayerStart and all environment actor transforms are preserved. The pre-existing `BP_Janyeol_C_1` placeholder actor was removed from this map only; its Blueprint and monster assets remain unchanged.

Direct PIE/OpenLevel resolves the current map's unique catalog stage and clears stale active quest-run context. If the catalog cannot resolve a stage, the GameMode's existing `DefaultRaidDefinition` fallback still points to Raid 03. There is no `DefaultStageId` property in this project.

While a required target is unset, the GameMode enters exploration without a raid run, target binding, encounter clock or loading hold. Start, completion, defeat and debug-finish hooks cannot settle a result. Returning to the lobby does not record an abandonment. Player movement, combat controls, environmental geometry and the normal HUD are retained; this mode is not player invulnerability.

## When the monster is ready

1. Set `DA_RaidStage03.TargetMonster` to the actual monster definition. Leave `RequireTargetMonster` enabled. Set the intended `TimeLimit` and presentation data if needed.
2. Place the intended monster Blueprint in `L_Raid03`, with its monster definition matching `TargetMonster`, as Raid 01/02 do. The raid definition selects placed targets; it does not spawn a boss by itself. No placeholder spawn configuration was copied.
3. Set `DA_Quest_Raid03_Normal.BossDefinition` to that same definition and `BossId` to its authoritative Codex boss ID. Add/update the corresponding row in the Codex asset referenced by `DA_QuestCatalog.CodexData` so its BossId and MonsterDefinition agree.
4. Configure this quest's own `ClearRewards` (required by the current validator), any `PartBreakRewards` with real part tags, and intended money, socket and prerequisite values. Keep quest rewards in the quest definition; do not populate legacy raid Rewards to support this quest.
5. Enable `DA_Quest_Raid03_Normal.ContentEnabled` after data validation. The stage and map placement are already connected; no GameFlow code or ID changes are needed. The stage is not DevelopmentOnly.

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
