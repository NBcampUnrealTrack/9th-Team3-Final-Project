# Knight sandstorm

## Placement and editing

- Active quest routing was read from the current assets: `Quest_Janyeol_Normal` → `Stage_01` → `/Game/Knight/Maps/L_Raid01`.
- Placed actor: **Knight Sandstorm - toggle Enabled for comparison**, at `(-334400, -382400, 1750)` cm.
- Blueprint: `/Game/Knight/Environment/Sandstorm/BP_KnightSandstorm` (native parent `AKnightSandstorm`).
- Select this actor in the level and edit its **Sandstorm** categories. Blueprint callers can use `SetSandstormEnabled(bool)` or change presentation properties then call `ApplySettings()`.
- No save-game, camera, exposure, lighting, combat or UI settings are changed by the effect.

| Control | Initial value | Meaning |
|---|---:|---|
| Enabled / Intensity | true / 1 | OFF restores the captured original fog; zero intensity also restores it |
| Near Clear Distance | 1500 cm | Ordinary height-fog start, measured from camera |
| Haze Transition Distance | 8000 cm | Density scale relative to an 80 m optical transition; not an invisibility cutoff |
| Fog Density | 0.60 | Multiplied by intensity and `8000 / HazeTransitionDistance` |
| Fog Tint | linear RGB 0.4375 / 0.42426 / 0.359892 | Preserves the original level's warm-neutral fog color |
| Fog Height Falloff | 0.001 | Shallow vertical change across the arena |
| Use Volumetric Fog | true | Uses the existing project's fog scalability |
| Volumetric Start / Near Fade | 1500 / 6000 cm | Separate controls from ordinary fog start |
| Volumetric View Distance | 20000 cm | Integration grid extent; **not** where objects become invisible |
| Wind Direction / Speed | `(1, .25, 0)` / 350 cm/s | World-space direction, normalized by the actor |
| Dust Amount / Size / Gust Strength | 1 / 1 / .25 | Rate, sprite scale, slow opacity gust variation |
| Coverage Extent | `(10000,10000,2000)` cm | Half-extents of the dust emission box |
| Light Dust | false | 35% spawn rate with exactly the same haze |

Keep ordinary and volumetric starts/fades deliberate when adjusting combat visibility: changing ordinary `Near Clear Distance` does not silently overwrite the separate volumetric settings. World units are centimeters. The initial player-start / boss separation was about 36 m.

## Ownership and restoration

`Target Fog` explicitly references the existing `ExponentialHeightFog_0`. The actor does not spawn another fog. It captures only the properties it owns into a serialized baseline; disabling, removal and EndPlay restore those properties. Re-enabling is idempotent. A second sandstorm controller targeting the same fog is rejected while the first owns it. To use a different fog, change `Target Fog`; the old target is restored first.

The fog is a world-wide height fog, as in Unreal's original implementation. **Coverage Extent bounds dust emission only; it does not localize height fog.** Do not add a competing weather controller to the same target. Existing `NS_StormDust_00A` and `NS_StormClouds_00A` actors remain untouched.

## Assets and cost controls

Under `/Game/Knight/Environment/Sandstorm`:

- `M_KnightSandstormDust`: derived from the existing `M_Master_Dust_Flipbook` material, adding soft radial edge, camera clearance (250–700 cm) and slow gust modulation; original depth fading retained.
- `MI_KnightSandstormDust`: reuses `/Game/UnrealNormandy/Textures/T_DustParticle_01A_Mask`, with restrained opacity and emissive power.
- `NS_KnightSandstormDust`: two continuous lightweight Niagara emitters, `FineDust` and `SoftWisps`; analytic velocity and curl-noise motion, lifetime opacity ramps, no lights/collision/fluids.
- `BP_KnightSandstorm`: assigned system and material, ready to place.

Actor-to-system bindings: `User.FineRate`, `User.WispRate`, `User.BoxSize`, `User.WindVelocity`, `User.SizeScale`, `User.DustMaterial`. Defaults are 160 fine particles/s (2–4 s life) and 12 wisps/s (4–6 s life). Actor clamps rates to 240 and 20 respectively: at most about 1080 live particles at those maximum rates/lifetimes. Light mode defaults to 56 and 4.2 particles/s. Bounds include emission extents plus 100 m of drift/sprite margin. The actor has no tick and does not spawn replacement components each frame.

## Verification route

`knight.SandstormCheck <output-directory>` is an editor-only, explicit opt-in render comparison. It requires a `-KnightSaveTestNamespace=Sandstorm...` argument. It uses the real player camera, stops AI/movement without attacking, captures OFF/ON/Light states and CSV profiles, then restores temporary FPS/V-Sync overrides and exits. It does not create/load user profiles or save presentation values. This is not an interactive combat test.

Task evidence lives in `Saved/Codex/Sandstorm_20261026`. The pre-edit map backup is `L_Raid01.before.umap`; do not copy it over later unrelated changes.

## Verified on 2026-10-26

- `knightEditor Win64 Development` succeeded (`build_final.log`). Material recompilation, Niagara compilation and Blueprint compilation completed; final saved references and Blueprint status were re-read by the editor commandlet (`verify_final.log`, `verified.json`).
- OFF restored the original density 0.036666 / height falloff 0.035 / start 0 / volumetric distance 300000 / extinction 2.08 and the original color. Repeated ON was idempotent. Light mode retained identical fog settings. There is one height-fog actor in the map.
- Genuine D3D12 game rendering, with no desktop input automation: `render_final/01_off.png`, `02_on.png`, `03_light.png`. All are **1280×720**, real player camera `(-336643.502,-382295.592,659.174)`, rotation `(-7,-1.531632,0)`, FOV 85. Nearby player/boss silhouettes remain distinguishable; distant cliffs lose contrast with ON. Existing animated storm clouds/particles continue between captures, so these are not identical animation frames.
- RTX 3060 Ti 8 GB, driver 617.42, Effects/Shadow quality 3; DLSS enable 0, GPU CSV includes the TSR pass. No global quality setting was changed. Output resolution is confirmed; internal resolution was not measured. Ordinary fog start is not a guaranteed opacity cutoff, and volumetric view distance is not an invisibility distance ([Epic documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine)).

Eight-second samples after initial loading, median milliseconds:

| State | Frames | Frame | Game | Render | GPU | GPU volumetric fog |
|---|---:|---:|---:|---:|---:|---:|
| Original fog / new dust OFF | 755 | 10.304 | 3.298 | 10.289 | 9.570 | 0.423 |
| Sandstorm ON | 742 | 10.451 | 3.274 | 10.428 | 9.670 | 0.481 |
| Light Dust | 745 | 10.415 | 3.335 | 10.394 | 9.700 | 0.481 |

These short stationary editor-game samples are not packaged or combat benchmarks. The light option lowers configured emission but did **not** demonstrate a meaningful GPU improvement in this sample. Raw CSVs are in `final_csv`, summaries in `final_perf.json`. Startup's approximately 250-second map loading is excluded, not attributed to this effect. Log view-ray samples at 65–71 m hit existing BlockingVolumes, not the rendered cliff surfaces; do not use them as terrain-depth measurements.

The comparison process exited normally, restored runtime 60 FPS / V-Sync OFF, and did not save these overrides. Original user save hashes (46 files) remained unchanged. Existing toolset Python initialization errors under `-game`, GameplayCue scan delay and the Knight/Knight_v2 body-region warning remain outside this effect's scope. Unreal MCP was not available in this session; asset operations used Unreal's Python commandlet directly.

Still unverified: moving-camera culling across the entire dust box, real attack readability, other graphics presets/GPUs and packaged execution. Existing `NS_StormDust_00A` / `NS_StormClouds_00A` visuals remain present in both OFF and ON captures; OFF disables this new effect, not all weather in the level.
