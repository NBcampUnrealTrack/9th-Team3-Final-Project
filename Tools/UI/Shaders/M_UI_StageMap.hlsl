// M_UI_StageMap Custom 노드 코드. 입력: UV, ViewRect, MapPixels, Tex(LandTex), Blink, R0..R7(Region0..7). 출력 float4.
// 고친 뒤 Tools/UI/make_stage_map_material.py로 에셋에 넣는다.

// Map texture coordinates of this pixel, and the same in map pixels (Slate units at the current zoom).
float2 uv = ViewRect.xy + UV * ViewRect.zw;
float2 px = uv * MapPixels.xy;

float3 bg = float3(0.0033, 0.0056, 0.0091);
float3 frost = float3(0.39, 0.58, 0.87);
float3 ember = float3(1.0, 0.195, 0.010);
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
    bool selected = st > 2.5;
    float3 regionCol = locked ? grey : (selected ? ember : frost);
    float fill = locked ? 0.14 : (selected ? 0.15 : (st > 1.5 ? 0.16 : 0.09));
    if (selected && !locked)
    {
        // Blink: +1 bright, -1 dim, 0 steady.
        fill = Blink > 0.5 ? 0.4 : (Blink < -0.5 ? 0.03 : fill);
    }
    float edge = locked ? 0.55 : (selected ? 0.95 : (st > 1.5 ? 0.8 : 0.45));
    if (locked)
    {
        // Dashed outline: 24 dashes around the region.
        edge *= step(0.5, frac(bestAngle * 24.0 / 6.2831853));
    }
    col = lerp(col, regionCol, fill * inRegion);
    col = lerp(col, regionCol, edge * edgeBand);
    dotCol = lerp(dotCol, locked ? grey * 2.0 : regionCol, inRegion * (selected ? 0.6 : 0.4));
}
col += frost * grid * 0.06;
col = lerp(col, dotCol, dotMask * dotAlpha);

// Soft fade at the widget edges so the map reads as a panel.
float2 e = min(UV, 1.0 - UV);
float edgeFade = smoothstep(0.0, 0.03, min(e.x, e.y));
col *= 0.6 + 0.4 * edgeFade;
return float4(col, alpha);
