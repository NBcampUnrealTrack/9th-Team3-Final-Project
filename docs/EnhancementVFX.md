# Enhancement fragment VFX

The large selected-weapon preview in Maintenance → Enhancement uses dedicated UI materials. Previously each fragment was a uniformly tinted `FSlateColorBrush` rectangle, with no light or trail mask. The weapon-list icons, gameplay upgrade rules, costs, saves and other maintenance screens are unchanged.

## Assets and integration

The reusable assets are under `/Game/Knight/UI/Menu/Maintenance/Enhancement/VFX/` (unchanged by the exit refinement):

| Asset | Purpose |
| --- | --- |
| `M_UI_EnhancementFragment` | Additive User Interface material with procedural luminous core, Gaussian halo and directional fading trail masks. No scene bloom, texture streaming, Niagara UI plugin or scene capture is required. |
| `MI_UI_EnhancementCore` | Bright amber fragment preset. |
| `MI_UI_EnhancementEcho` | Dimmer, longer fragment preset. |
| `M_UI_EnhancementFieldGlow` | Restrained elliptical light behind the individual fragments. |

`/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance` holds hard references in `EnhancementFragmentMaterial`, `EnhancementEchoMaterial`, and `EnhancementFieldGlowMaterial`. The existing `/Game/Knight/UI` cook rule also covers these packages; no cook rule was broadened.

`KnightMaintenanceEnhancement.cpp` builds 318 fragment images and 24 shared dynamic palettes, plus one field-glow image. The field uses a fixed irregular distribution, independently staggered gathering, short stepped jitters and opacity variation. Each fragment retains its identity, position and alpha when the buffered real result begins. Success accelerates upward; failure transitions to red and accelerates downward. Trails extend with departure speed and fade with the fragment.

The effect is a sibling of the icon's ScaleBox, inside a zero-desired-size wrapper. Its movement cannot alter the weapon's layout or ScaleToFit aspect ratio. The weapon's 320 × 180 presentation, surrounding layout and authored render transforms are preserved. Normalized positions and quad sizes scale with the preview/DPI.

### Travel space and clipping

The previous 218px effect canvas (`ClipToBoundsAlways`) and its enclosing detail ScrollBox (`ClipToBounds`) cut moving quads off at the level/readout rows. The old `1 - T*T` opacity retained considerable brightness until late in the result, and cleanup could collapse the canvas before submitting a fully transparent frame. The procedural material already fades its UV edges to zero; its existing core/halo/trail masks are retained.

The preview now paints in a root-canvas sibling **outside** the ScrollBox. A 320 × 180 spacer reserves the exact original layout footprint; `SyncEnhancementVisualLayout()` mirrors its geometry onto the actual preview without moving or transforming the weapon. The travel canvas extends **230px above** and **260px below** the icon layout (670px total at unit draw scale). At 1920 × 1080 this is y=134..804, above both the weapon name and level, and well below the sword. The charging field remains 218px high at its original location, and bar/halo/trail quad dimensions are unchanged. Only fragment placement and lateral jitter are multiplied by **0.88**, giving a centered **12% reduction** in horizontal spread.

Layer order is artwork → readout background → weapon/VFX → existing foreground columns/text. The readout background's separate clip host mirrors the original ScrollBox viewport so its frame/text stay in their original locations. Name, level and material/chance text paint above passing fragments.

Result lifetimes vary from 76% to 100% of the release duration. All components receive the same smooth per-fragment opacity `pow(1 - smoothstep(T), 1.3)`, while movement accelerates up/down. A second smooth fade starts before the **entire padded quad** (core, halo and trail) reaches the travel boundary. The central glow fades smoothly too. A dedicated `Transparent` phase submits and holds zero alpha for 0.08s before collapsing/resetting anything. Creation/reset also clears image tint opacity so an uninitialized first layout cannot expose default white tints or old results.

## Tuning

- WBP defaults: `EnhancementGatherDuration` (0.46 s), `EnhancementHoldDuration` (0.28 s), `EnhancementReleaseDuration` (0.86 s), followed by a fixed 0.08s transparent hold.
- Fragment material: independent `CoreBrightness`, `CoreOpacity`, `CoreLength`; `HaloBrightness`, `HaloOpacity`, `HaloLength`, `HaloWidth`; `TrailBrightness`, `TrailOpacity`, `TrailLength`, `TrailDirection`; and `CoreColor` / `GlowColor`.
- Runtime length variation multiplies the authored core/trail values. Runtime outcome color/direction and trail extension animate on the shared palettes. Per-fragment fade/jitter uses the image tint and canvas position.
- Field material: `Brightness`, `Opacity`, `GlowColor`. Keep this light restrained so gaps remain visible.

`RequestWeaponUpgrade()` still calls `TryUpgradeWeapon()` once, then passes its returned enum to the presentation. No animation tick/end/reset path rolls, deducts costs or requests a save. Rejection starts no effect; an active phase blocks a second request. Existing weapon-selection, profile, open, close and destruct paths reset presentation only.

## Reproduction and verification

The editor-only `knightEnhancementVFXAssets` commandlet authors these four dedicated material assets, binds the three WBP properties, compiles the materials/WBP and saves only those packages. Re-running it replaces their authored defaults; preserve intentional artist tuning before regenerating.

```powershell
& 'D:\Unreal\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Unproject\knight\knight.uproject' -run=knightEnhancementVFXAssets `
  -AllowCommandletRendering -unattended -nosplash -UserDir=<isolated-test-directory> -abslog=<external-log>
```

Read-only rendered regression check:

```powershell
& 'D:\Unreal\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Unproject\knight\knight.uproject' -run=knightEnhancementPreviewCheck `
  -FullUI -Frames -Out=<external-capture-directory> -AllowCommandletRendering `
  -unattended -nosplash -UserDir=<isolated-test-directory> -abslog=<external-log>
```

Omit `-Frames` for only key captures. Omit `-FullUI` for the isolated preview component. `-DrawScale=0.75` or `1.5` exercises scaled Slate layout. Captures use the actual production enhancement layout, artwork, weapon texture, fragment materials and runtime animation code through `FWidgetRenderer` on D3D12. Readback avoids double gamma encoding.

The harness injects success/failure enums and quote text fixtures without loading a profile or running an enhancement transaction. Its full-layout mode renders the native enhancement columns/artwork, without the outer WBP menu chrome, player/action button, owned-weapon list population or gameplay world. It checks fixed icon geometry/transforms on each frame, result continuity, repeated input guard, rejection, repeated failure, completion cleanup and interrupted nonzero idle transforms. It also checks expanded travel bounds, foreground text layer order, visible fragments reaching the upper/lower travel areas, complete quad bounds, monotone result alpha and multiple fully transparent frames before cleanup. This evidence does not establish live player input, economy/save behavior, full menu navigation or packaged behavior.

Exit-refinement evidence is stored outside the repository in `D:\Builds\Knight\EnhancementExit_20261104\`; the original luminous-material baseline remains in `D:\Builds\Knight\EnhancementLuminous_20261104\`. No Computer Use, packaging, commit or push is part of this workflow.

## Completed exit-refinement checks (2026-11-04)

Evidence below is in `D:\Builds\Knight\EnhancementExit_20261104\`:

- Installed association `5.8`; actual engine `D:\Unreal\UE_5.8` reports **5.8.4**, changelist 58902052. `knightEditor Win64 Development` built successfully (`build_final.log`).
- Maintenance WBP compiled read-only with **0 errors, 0 warnings** (`widget_compile.log`). No content asset was rewritten. Existing toolchain plugin-dependency warnings remain outside this change.
- D3D12 rendered regression checks at **75%, 100%, 150%** Slate draw scale each returned **failures=0** (`scale075.log`, `verified_render.log`, `scale150.log`). Each run checked success, two successive failures, rejection, repeat-request guard, interruption, complete visible quad bounds, upper/lower travel, monotone alpha and multiple zero-alpha frames before cleanup.
- The original idle screenshot and refined idle screenshot are pixel-identical. The cleaned screenshot is also pixel-identical. Weapon, labels, artwork and panel placement remain unchanged (`capture_report.json`).
- The charging capture's visible width fell from 737px to 647px (about **12.2%**, including unchanged halo padding). Placement itself is multiplied by exactly **0.88**. Charging anchors/quad height remain unchanged; their previously cropped soft outer pixels now have space to render.
- Last visible 8-bit result remnants occur at frame 91 (success) and 92 (failure); subsequent frames match idle through frame 129. Difference energy falls progressively over the final visible frames. The first success frame also matches idle, confirming no uninitialized first-paint flash (`capture_report.json`).
- `VerifiedUI/` contains the actual rendered frames. `enhancement_smooth_exit.mp4` is a **5.53-second** crop of those frames: success then failure, fixed 1/60-second simulation steps and brief idle padding. This is an offline render, not a gameplay frame-rate measurement. H.264 decode passed (`video_decode.log`).
- `git diff --check` passed. No profiles/saves were loaded or edited; testing used a separate `UserDir`. Live gameplay/menu navigation and packaged execution remain unverified.

Changed files in this refinement:

- `Source/knight/Menu/UI/KnightMaintenanceEnhancement.cpp`: detached visual layers, travel/charging separation, width, smooth fade and transparent cleanup.
- `Source/knight/Menu/UI/KnightMaintenanceWidget.cpp`: detached visual destination visibility.
- `Source/knight/Menu/UI/KnightMaintenanceWidget.h`: visual references and transparent phase; release default 0.86s.
- `Source/knightEditor/knightEnhancementPreviewCheckCommandlet.cpp`: expanded render bounds and full sequence/cleanup checks.
- `Source/knightEditor/knightEnhancementVFXAssetsCommandlet.cpp`: renamed its generic folder constant to avoid a unity-build name collision; asset authoring behavior is unchanged.
- `docs/EnhancementVFX.md`: implementation and evidence notes.

The four VFX assets and maintenance WBP remain unchanged on disk.
