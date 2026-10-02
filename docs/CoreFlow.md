# Knight core flow and save contracts

## Ownership

- `UKnightGameInstance` initializes the existing `UKnightGameFlowSubsystem`, `UknightRaidSubsystem`, `UknightProgressionSubsystem`, and `UknightSaveSubsystem`. Each subsystem owns its own state.
- `UKnightGameFlowSubsystem` owns map travel and the stage catalog. The catalog's `StageId`, name, and map are canonical. `UknightRaidDefinition` supplies raid rules, a cleared-stage prerequisite, and rewards. The asset manager streams the definition before travel; only GameMode registration confirms arrival.
- `UknightRaidSubsystem` is the offline run context (selected stage, GUID run ID, last result). It has no online-session behavior. `AknightRaidGameMode` alone finalizes a result; `AknightRaidGameState` publishes the phase to UI.
- `UknightProgressionSubsystem` applies a run ID once and owns items, upgrades, stage records, and unlocks. `UknightSaveSubsystem` owns slot loading and serialized snapshots. The existing `UknightSaveGame` layout is version 3; version 1 item migration remains supported.
- The existing `AKnightMenuGameMode` handles both title and UI lobby through its Blueprint flow-state setting. `AKnightMenuPlayerController` uses UI input, while `AknightPlayerController` uses game input. On lobby return, the existing main-menu status line presents the last raid outcome once. No second menu travel owner was introduced.

## Profile selection and persistence

- On the title screen, **Start** and **Save** open the existing SaveSelect page. Enter a slot ID and choose **프로필 선택**. `Default` uses the original `KnightProgress` file; other IDs use `KnightProgress_<ID>`. The page shows only the entered ID and whether a file exists. IDs must be 1–48 letters/digits/underscores/hyphens.
- An existing slot is read and validated before menu travel. An unreadable slot stays on the title screen, preserves its file, and allows another selection. A new slot starts with version-3 defaults; the title waits for its initial asynchronous save to succeed before opening the lobby. Failed writes keep the new in-memory profile available for retry.
- The lobby displays the active slot and offers **프로필 변경**, which returns to the title. Switching while a save is in flight is rejected. The save subsystem keeps one immutable snapshot in flight, coalesces later save requests into a follow-up snapshot, and emits `OnSaveFinished` when the queue drains. A callback is accepted only for the in-flight slot. Progression changes, including raid results, request a save automatically.
- The catalog marks `Core_Raid_Test` as development-only. It is labeled `[DEV]` in editor PIE and omitted from the normal stage list outside PIE. The test map and its assets remain available for direct validation.

## Team entry points

1. Add a unique `StageId` to `/Game/Knight/Data/DA_StageCatalog`, with a map using `AknightRaidGameMode` and a `UknightRaidDefinition`. Set `RequiredClearedStageId` only when a previous clear should unlock it.
2. Call `UKnightGameFlowSubsystem::CanLaunchStage(StageId, Reason)` and then `RequestStartStage(StageId)` from the lobby. An accepted request means loading/travel started; `GetActiveStageId()` is valid only after the destination GameMode registers.
3. The raid GameMode may call `RequestStartRaid()`, `NotifyObjectiveCompleted()`, `NotifyPlayerDefeated()`, `AbandonRaid()`, or `RequestReturnToLobby()`. Repeated finish calls cannot replace the first result. The menu reads `UknightRaidSubsystem::GetLastResult()` and clears it after presentation.
4. Use `UknightProgressionSubsystem` to query or change items, upgrades, clear records, and unlocks. `UknightSaveSubsystem::SelectSlot(SlotId)` chooses a profile (letters, digits, `_`, `-`; `Default` preserves the old `KnightProgress` slot). Check `DidLoadFail()` and `OnSaveFinished`; a failed load leaves that slot unwritable until an explicit reset or another slot selection.
5. `IGenericTeamAgentInterface` is the engine interface. `knightTeams` assigns Player 1, Monster 2, Ally 3, Neutral 255; player and ally are friendly. Combat and target interfaces retain the native methods used by current characters and expose `BlueprintNativeEvent` entry points. Call `ICombatInterface::Execute_CanReceiveHitBP`, `ITargetableInterface::Execute_CanBeTargeted` and related `Execute_` methods on Blueprint actors. `IInteractableInterface` is fully Blueprint implementable.

## Development validation

`/Game/Knight/Dev/Core/L_RaidCoreTest` and `DA_RaidCoreTest` are isolated test content. The catalog entry `Core_Raid_Test` awards two `DA_CoreTestMaterial` items on success and none on failure. In PIE, open title → lobby → stage → Core Raid Test → launch. The test controller accepts `KnightCoreSelectTestSlot Core_20261002_01`, `KnightCoreStatus`, `KnightCoreSuccess`, `KnightCoreFailure`, and `KnightCoreProbeInterfaces` through the editor's **Cmd** box. Use a distinct test slot for another run. The Blueprint probe overrides `CanInteract` to false; the native combat and target defaults return true.

For a persistence check, finish a raid, wait for `Saved/SaveGames/KnightProgress_<slot>.sav`, stop PIE, start a new PIE session, select the same slot, and compare `KnightCoreStatus`. Do not use the default slot for destructive tests. The development map has no monster; use the explicit test commands to finish it. A production raid map needs a placed or spawned target monster, gameplay pawn/controller, and presentation assets from their owners.

Editor-only validation commands: from the lobby, `KnightCoreFailNextRaidLoad`, `KnightCoreLaunchTwice Core_Raid_Test`, then `KnightCoreLaunch Core_Raid_Test` exercise duplicate rejection and retry after a load failure. In the test raid, `KnightCoreSaveQueueTest <another-slot>` requests two snapshots and tries a forbidden switch; `KnightCoreFailNextSaves 1` makes the next save fail without writing; `KnightCoreSaveOnce` retries. A real write failure can be tested in a fresh isolated slot by temporarily placing an empty directory at that slot's `.sav` path, then removing the directory and retrying. Use exact test paths and preserve all other saves. The short invalid-file test covers only a missing/invalid header, not arbitrary GVAS body corruption.
