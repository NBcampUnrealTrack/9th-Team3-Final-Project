# Enhancement fragment VFX

The large selected-weapon preview in Maintenance → Enhancement uses dedicated UI materials. Previously each fragment was a uniformly tinted `FSlateColorBrush` rectangle, with no light or trail mask. The weapon-list icons, gameplay upgrade rules, costs, saves and other maintenance screens are unchanged.

## Assets and integration

All new assets are under `/Game/Knight/UI/Menu/Maintenance/Enhancement/VFX/`:

| Asset | Purpose |
| --- | --- |
| `M_UI_EnhancementFragment` | Additive User Interface material with procedural luminous core, Gaussian halo and directional fading trail masks. No scene bloom, texture streaming, Niagara UI plugin or scene capture is required. |
| `MI_UI_EnhancementCore` | Bright amber fragment preset. |
| `MI_UI_EnhancementEcho` | Dimmer, longer fragment preset. |
| `M_UI_EnhancementFieldGlow` | Restrained elliptical light behind the individual fragments. |

`/Game/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance` holds hard references in `EnhancementFragmentMaterial`, `EnhancementEchoMaterial`, and `EnhancementFieldGlowMaterial`. The existing `/Game/Knight/UI` cook rule also covers these packages; no cook rule was broadened.

`KnightMaintenanceEnhancement.cpp` builds 318 fragment images and 24 shared dynamic palettes, plus one field-glow image. The field uses a fixed irregular distribution, independently staggered gathering, short stepped jitters and opacity variation. Each fragment retains its identity, position and alpha when the buffered real result begins. Success accelerates upward; failure transitions to red and accelerates downward. Trails extend with departure speed and fade with the fragment.

The effect is a sibling of the icon's ScaleBox, inside a zero-desired-size wrapper. Its movement cannot alter the weapon's layout or ScaleToFit aspect ratio. It uses the existing 18 px upper and 20 px lower gaps, clipped before the level and readout. The weapon's 320 × 180 presentation, surrounding layout and authored render transforms are preserved. Normalized positions and quad sizes scale with the preview/DPI.

## Tuning

- WBP defaults: `EnhancementGatherDuration` (0.46 s), `EnhancementHoldDuration` (0.28 s), `EnhancementReleaseDuration` (0.52 s).
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

The harness injects success/failure enums and quote text fixtures without loading a profile or running an enhancement transaction. Its full-layout mode renders the native enhancement columns/artwork, without the outer WBP menu chrome, player/action button, owned-weapon list population or gameplay world. It checks fixed icon geometry/transforms on each frame, result continuity, repeated input guard, rejection, repeated failure, completion cleanup and interrupted nonzero idle transforms. It also checks that the effect bounds exclude level/readout text. This evidence does not establish live player input, economy/save behavior, full menu navigation or packaged behavior.

Evidence for this change is stored outside the repository in `D:\Builds\Knight\EnhancementLuminous_20261104\`. No Computer Use, packaging, commit or push is part of this workflow.

## Completed checks (2026-11-04)

- Installed association `5.8`; actual engine `D:\Unreal\UE_5.8` reports **5.8.4**, changelist 58902052. `knightEditor Win64 Development` built successfully (`build_final.log`).
- Dedicated materials compiled on D3D12 SM6; maintenance WBP compiled with **0 errors, 0 warnings** (`assets.log`). Existing toolchain plugin-dependency warnings remain outside this change.
- Rendered regression checks at **75%, 100%, 150%** Slate draw scale each returned **failures=0** (`scale075.log`, `verified_render.log`, `scale150.log`). Each run checked success, two successive failures, rejection, blocked repeat request and interruption.
- At 1920 × 1080 the changed pixels stayed inside the central preview's effect band (charging bounds x=553..1289, y=346..563). Name/level, chance and material text remained readable. The cleaned image exactly matches the idle image (`capture_report.json`).
- `VerifiedUI/` contains actual rendered PNG frames and key captures. `enhancement_success_failure.mp4` is a 4.37-second crop of these frames: success followed by failure, fixed 1/60-second simulation steps, brief idle padding, no speed change to the animation. This is an offline render, not a gameplay frame-rate measurement. H.264 decode passed (`video_decode.log`).
- `git diff --check` passed. No profiles/saves were loaded or edited; testing used a separate `UserDir`.

Changed source/documentation files:

- `Source/knight/Menu/UI/KnightMaintenanceEnhancement.cpp`
- `Source/knight/Menu/UI/KnightMaintenanceWidget.h`
- `Source/knightEditor/knightEnhancementVFXAssetsCommandlet.cpp` and `.h` (new)
- `Source/knightEditor/knightEnhancementPreviewCheckCommandlet.cpp`
- `docs/EnhancementVFX.md` (new)

Changed content consists only of the four VFX assets listed above (new) and `Content/Knight/UI/Menu/Maintenance/WBP_KnightMaintenance.uasset` (the three hard references).
