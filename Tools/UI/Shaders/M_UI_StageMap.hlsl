// UI custom node: UV, ViewRect, MapPixels, Tex, R0..R7, FX0..FX7, SelectionTuning. Output float4.
// Blink is retained as an unused serialized input for compatibility.
// 고친 뒤 Tools/UI/make_stage_map_material.py로 에셋에 넣는다.

// Map texture coordinates of this pixel, and the same in map pixels (Slate units at the current zoom).
float2 uv = ViewRect.xy + UV * ViewRect.zw;
float2 px = uv * MapPixels.xy;

float3 bg = float3(0.0033, 0.0056, 0.0091);
float3 frost = float3(0.39, 0.58, 0.87);
float3 blue = float3(0.19, 0.56, 1.0);
float3 grey = float3(0.19, 0.23, 0.28);

// Dot grid: one dot per cell, sized and lit by the land under the cell centre so each dot is uniform.
const float Cell = 9.0;
float2 cellId = floor(px / Cell);
float2 cellCentre = (cellId + 0.5) * Cell;
float3 m = Texture2DSampleLevel(Tex, TexSampler, cellCentre / MapPixels.xy, 0).rgb;
float land = smoothstep(0.35, 0.65, m.r);
float dotRadius = lerp(0.9, 1.7, land);
float dist = length(px - cellCentre);
float aa = max(fwidth(dist), 1e-3);
float dotMask = 1.0 - smoothstep(dotRadius - aa, dotRadius + aa, dist);
float dotAlpha = lerp(0.10, 0.50 + 0.25 * m.b, land) + m.g * 0.25;

// Faint grid every 30 degrees.
float2 deg = float2(uv.x * 360.0, uv.y * 180.0) / 30.0;
float2 gd = abs(frac(deg + 0.5) - 0.5);
float2 gw = max(fwidth(deg), 1e-4);
float grid = 1.0 - min(smoothstep(0.0, gw.x * 1.5, gd.x), smoothstep(0.0, gw.y * 1.5, gd.y));

// Stage regions: soft shapes around each stage, edge wobbled the same way as UKnightStageMapWidget::FindMarkerAt.
// R = (u, v, radius in map pixels, state). State 0 unused, 1 normal, 2 hovered, 3 selected, +3 locked.
float4 regions[8] = { R0, R1, R2, R3, R4, R5, R6, R7 };
float4 effects[8] = { FX0, FX1, FX2, FX3, FX4, FX5, FX6, FX7 };
float4 bestFX = 0;
float bestRadius = 1.0;
float bestT = 1e4;
float bestState = 0.0;
float bestAngle = 0.0;
for (int i = 0; i < 8; ++i)
{
    float4 g = regions[i];
    if (g.w < 0.5 || g.z <= 0.0)
    {
        continue;
    }
    float2 d = px - g.xy * MapPixels.xy;
    float a = atan2(d.y, d.x);
    float wob = 1.0 + 0.12 * sin(3.0 * a + g.x * 40.0) + 0.07 * cos(5.0 * a + g.y * 40.0);
    float t = length(d) / (g.z * wob);
    if (t < bestT)
    {
        bestT = t;
        bestState = g.w;
        bestAngle = a;
        bestFX = effects[i];
        bestRadius = g.z * wob;
    }
}

float3 col = bg;
float alpha = 0.92;
float3 dotCol = frost;
float tw = max(fwidth(bestT), 1e-4);
float inRegion = 1.0 - smoothstep(1.0 - tw, 1.0 + tw, bestT);
float edgeBand = 1.0 - smoothstep(0.0, tw * 1.5, abs(bestT - 1.0));
if (bestState > 0.5)
{
    bool locked = bestState > 3.5;
    float st = locked ? bestState - 3.0 : bestState;
    float selected = saturate(bestFX.x);
    float hovered = saturate(bestFX.w) * (1.0 - selected);
    float3 regionCol = locked ? grey : frost;
    float fill = locked ? 0.14 : 0.09;
    float edge = locked ? 0.55 : 0.45;
    if (locked) edge *= step(0.5, frac(bestAngle * 24.0 / 6.2831853));
    col = lerp(col, regionCol, fill * inRegion);
    col = lerp(col, regionCol, edge * edgeBand);
    // Selection and hover have different strengths; availability does not mask selection.
    col = lerp(col, blue, inRegion * selected * SelectionTuning.z);
    col += frost * hovered * (inRegion * 0.025 + edgeBand * 0.13);
    dotCol = lerp(dotCol, locked ? grey * 2.0 : regionCol, inRegion * 0.4);
}
col += frost * grid * 0.06;
col = lerp(col, dotCol, dotMask * dotAlpha);
// The moving segment uses the exact same irregular perimeter as the artwork and hit test.
// No extra rectangle, texture crop, particle layer, or independent screen coordinates.
float edgeDistance = abs(bestT - 1.0) * bestRadius;
float core = 1.0 - smoothstep(0.6, 2.1, edgeDistance);
float halo = exp(-pow(edgeDistance / max(SelectionTuning.y, 1.0), 2.0));
float angle01 = bestAngle / 6.2831853 + 0.5;
float arcDistance = abs(frac(angle01 - bestFX.y + 0.5) - 0.5);
float segment = 1.0 - smoothstep(0.015, 0.075, arcDistance);
float selected = saturate(bestFX.x);
float peak = bestFX.z;
col += blue * selected * (core * SelectionTuning.x + halo * 0.055);
col += float3(0.40, 0.76, 1.0) * peak * (core * (0.24 + segment * 1.1) + halo * segment * 0.27);


// Soft fade at the widget edges so the map reads as a panel.
float2 e = min(UV, 1.0 - UV);
float edgeFade = smoothstep(0.0, 0.03, min(e.x, e.y));
col *= 0.6 + 0.4 * edgeFade;
return float4(col, alpha);
