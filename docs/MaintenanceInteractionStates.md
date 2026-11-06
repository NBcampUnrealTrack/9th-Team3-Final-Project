# Maintenance interaction states — 2026-11-05

Implemented on `feat/UI_Anim1105`. The working tree was clean before this pass.
Engine association is `5.8`; the installed engine used for validation is UE **5.8.4**, CL 58902052.

## Visual meanings

| State | Presentation | Authority |
| --- | --- | --- |
| Hover / navigation focus | Modest neutral text/icon brightening; navigation focus also has a thin neutral outline | Current pointer or keyboard/controller input, independent of inspection |
| Selected | Persistent 3-unit orange left edge | Existing owner-controlled CommonUI selection |
| Equipped | Small pale neutral-green `장착 중` caption at upper right | Active profile's resolved equipment identity |
| Locked / disabled | Readable gray label, no input highlight or motion; existing lock reason/validation retained | Existing availability rules |

The new visual layer contributes no desired size and intercepts no input. Row sizes,
label alignment, full-width equipment rows and external action geometry are retained.
The five local Holo styles retain their original idle artwork, padding and dimensions;
hover and selection no longer brighten the entire background. No new idle fill is added.

Equipped captions are enabled only for replacement candidates and the enhancement
weapon list. Equipment slots and crafting recipes do not receive redundant captions.
The current inventory groups equipment by `FPrimaryAssetId`; there is no separate
per-copy equipment instance identity in this path. Enhancement uses
`GetMainEquipmentSlots`, including explicit empty slots and unresolved-definition
handling; replacement uses the existing `GetEquippedInSelectedSlot` resolver.
Neither selection nor presentation invokes an equipment or progression mutation.

Input-mode feedback switches on mouse movement/click and navigation keys/focus
(including controller navigation focus). Pointer activity suppresses stale keyboard
outlines; navigation suppresses an old pointer hover. Selection and equipped captions
survive those changes. Picker refresh preserves scroll and focused item, and does not
steal focus from the confirmation/cancel actions.

Existing crafting-result transactions, equipment rules, enhancement transactions/VFX,
and enhancement-shortcut return routing remain in their original paths. Animated fill
opt-in still occurs only in title and main menu. Maintenance explicitly disables it.

## Changed files

- `Source/knight/Menu/UI/KnightMaintenanceStateExtension.h`
- `Source/knight/Menu/UI/KnightMaintenanceStateExtension.cpp`
- `Source/knight/Menu/UI/KnightMaintenancePresentation.h`
- `Source/knight/Menu/UI/KnightMaintenanceEntryWidget.cpp`
- `Source/knight/Menu/UI/KnightMaintenanceWidget.h`
- `Source/knight/Menu/UI/KnightMaintenanceWidget.cpp`
- `Source/knight/Menu/UI/KnightMaintenanceCraftResult.cpp` — input-modality observation only; dialog handling unchanged.
- `Source/knightUI/Widgets/knightButtonBase.cpp`
- `Source/knightEditor/knightEquipmentActionCheckCommandlet.cpp`
- `Tools/Editor/style_maintenance_states.py` — reproducible, narrowly scoped asset update.
- `Content/Knight/UI/Menu/Maintenance/Hologram/BS_Holo_Row.uasset`
- `Content/Knight/UI/Menu/Maintenance/Hologram/BS_Holo_Control.uasset`
- `Content/Knight/UI/Menu/Maintenance/Hologram/BS_Holo_MainTab.uasset`
- `Content/Knight/UI/Menu/Maintenance/Hologram/BS_Holo_Action.uasset`
- `Content/Knight/UI/Menu/Maintenance/Hologram/BS_Holo_Slot.uasset`
- This report.

## Validation

All logs/captures are outside the repository at
`D:/Builds/Knight/InteractionStates_20261105`.

- `knightEditor Win64 Development`: **Succeeded**, `Build5.log`.
- `knightEquipmentActionCheck`: **876 passed, 0 failed**, `Equipment2.log`, process exit 0.
  Uses production widgets and equipment APIs with isolated profiles. Checks all five
  slot layouts, replacement/cancel/unequip, retained ownership, empty weapon behavior,
  external button hit geometry/navigation links, enhancement shortcut and return helpers,
  rapid selection, close/reopen, profile changes, and actual upgrade/refresh.
  New checks cover equipped A / selected B / focused or hovered C, selected+equipped,
  badge overlap, selection-edge placement, unavailable action readability, pointer
  enter/leave, input-mode switching, cancel focus retention and indicator updates.
- Real offscreen **D3D12 Slate/UMG rendering**, not NullRHI: `Captures2`.
  Layout/hit checks cover 1280×720, 1920×1080, 2560×1440, 3440×1440 and 125% UI scale.
  Representative state captures are at 1920×1080.
- `knightCraftResultCheck`: **114 passed, 0 failed**, `Craft.log`, process exit 0.
  Existing weapon/armor craft/equip, cancellation/Esc, replacement, rejected/rapid
  requests, cost/ownership, focus and refresh regression checks. Real renders in `CraftCaptures`.
- `knightCompileReport`: Maintenance WBP, entry WBP, common button WBP and all five
  modified styles: **0 errors, 0 warnings**, `Compile.log`, process exit 0.
- `git diff --check`: passed.

Tests used separate `KnightSaveTestNamespace` values and existing test-only UserDir
folders. No personal saves were edited. No packaging, commit or push was performed.

These are real UMG renders and automated Slate focus/pointer-event checks, not physical
mouse/controller sessions or full PIE runs. The offscreen equipment view does not
render the lobby's 3D character/background. Physical device navigation, full menu
Back/Esc integration and packaged runtime remain untested in this pass.

## Representative captures

- [Equipped A, selected B, focused C](D:/Builds/Knight/InteractionStates_20261105/Captures2/States_EquippedA_SelectedB_FocusedC.png)
- [Equipped A, selected B, hovered C](D:/Builds/Knight/InteractionStates_20261105/Captures2/States_EquippedA_SelectedB_HoveredC.png)
- [Selected and equipped; unavailable action](D:/Builds/Knight/InteractionStates_20261105/Captures2/States_SelectedAndEquipped_DisabledAction.png)
- [Enhancement: equipped and inspected weapons differ](D:/Builds/Knight/InteractionStates_20261105/Captures2/States_Enhancement_EquippedAndInspected.png)
- [Selected slot with focus on external action](D:/Builds/Knight/InteractionStates_20261105/Captures2/States_SlotSelected_ActionFocused.png)
- [Crafting result regression](D:/Builds/Knight/InteractionStates_20261105/CraftCaptures/WeaponResult.png)
