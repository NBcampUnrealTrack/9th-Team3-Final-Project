// M_UI_Globe Custom 노드 코드. 입력: UV, Yaw, Pitch, Radius, Tex(LandTex), Time, R0..R7(Region0..7). 출력 float4.
// 고친 뒤 Tools/UI/apply_globe_material.py로 에셋에 넣는다.

float2 p = UV * 2.0 - 1.0;
p.y = -p.y;
float r = length(p) / Radius;
float aa = max(fwidth(r), 1e-4);
// Thin atmosphere ring just outside the sphere.
float halo = saturate(1.0 - (r - 1.0) / 0.06) * step(1.0, r);
if (r > 1.0)
{
    return float4(0.45, 0.68, 1.0, halo * halo * 0.35);
}
float2 s = p / Radius;
float z = sqrt(saturate(1.0 - dot(s, s)));
float3 n = float3(s.x, s.y, z);
// Undo pitch (around X), then yaw (around Y). The widget's marker math is the exact inverse.
float cp = cos(Pitch), sp = sin(Pitch);
n = float3(n.x, cp * n.y + sp * n.z, -sp * n.y + cp * n.z);
float cy = cos(Yaw), sy = sin(Yaw);
n = float3(cy * n.x - sy * n.z, n.y, sy * n.x + cy * n.z);
float lat = asin(clamp(n.y, -1.0, 1.0));
float lon = atan2(n.x, n.z);
float2 uv = float2(lon / 6.2831853 + 0.5, 0.5 - lat / 3.1415927);
// Mip selection without the seam where longitude wraps.
float2 dx = ddx(uv); float2 dy = ddy(uv);
dx.x -= round(dx.x); dy.x -= round(dy.x);
float3 m = Texture2DSampleGrad(Tex, TexSampler, uv, dx, dy).rgb;
float landMask = m.r;
float coast = m.g;
float detail = m.b;

// Grid every 15 degrees.
float2 ll = float2(lon, lat) / 0.2617994;
float2 gd = abs(frac(ll + 0.5) - 0.5);
float2 gw = max(fwidth(ll), 1e-4);
gw.x = min(gw.x, 0.5);
float grid = 1.0 - min(smoothstep(0.0, gw.x * 1.5, gd.x), smoothstep(0.0, gw.y * 1.5, gd.y));

float3 ocean = float3(0.010, 0.030, 0.070);
float3 landCol = lerp(float3(0.10, 0.20, 0.36), float3(0.20, 0.34, 0.55), detail);
float3 col = lerp(ocean, landCol, landMask);
col += float3(0.45, 0.65, 0.95) * coast * 0.55;

// Stage regions: land within an angular radius of each stage, edges wobbled so they read as terrain, not circles.
// R = (longitude, latitude, radius in radians, state). State 0 unused, 1 normal, 2 hovered, 3 selected, +3 locked.
float4 regions[8] = { R0, R1, R2, R3, R4, R5, R6, R7 };
float bestT = 1e4;
float bestState = 0.0;
for (int i = 0; i < 8; ++i)
{
    float4 g = regions[i];
    if (g.w < 0.5 || g.z <= 0.0)
    {
        continue;
    }
    float3 c = float3(cos(g.y) * sin(g.x), sin(g.y), cos(g.y) * cos(g.x));
    float ang = acos(clamp(dot(n, c), -1.0, 1.0));
    float wob = 1.0 + 0.12 * sin(dot(n, float3(7.1, 3.3, -5.2)) + g.x * 3.0) + 0.08 * sin(dot(n, float3(-4.7, 9.1, 2.3)) * 1.7 + g.y * 5.0);
    float t = ang / (g.z * wob);
    if (t < bestT)
    {
        bestT = t;
        bestState = g.w;
    }
}
float tw = max(fwidth(bestT), 1e-4);
float inRegion = (1.0 - smoothstep(1.0 - tw, 1.0 + tw, bestT)) * smoothstep(0.35, 0.65, landMask);
// Other land steps back so the stage regions stand out.
if (bestState > 0.5)
{
    col = lerp(col, col * 0.55, landMask * (1.0 - inRegion));
}
if (inRegion > 0.0)
{
    bool locked = bestState > 3.5;
    float st = locked ? bestState - 3.0 : bestState;
    float3 baseCol = locked ? float3(0.10, 0.15, 0.22) : float3(0.28, 0.56, 0.92);
    float3 hiCol = locked ? float3(0.20, 0.27, 0.38) : float3(0.70, 0.87, 1.00);
    float3 regionCol = lerp(baseCol, hiCol, saturate(st - 1.0) * 0.5);
    // Relief: a plateau that rises steeply at the edge (bevel) with terrain detail on top, lit from the upper left
    // in screen space so it reads as raised whichever way the globe is turned.
    float h = saturate((1.0 - bestT) * 6.0) * smoothstep(0.35, 0.65, landMask);
    float2 gh = float2(ddx(h), ddy(h));
    float shade = 0.9 + clamp((gh.y - gh.x) * 60.0, -0.45, 0.45);
    float3 tinted = regionCol * shade * (0.85 + 0.3 * detail);
    // Bright rim along the region edge and the coast inside it; the selected rim pulses.
    float rimBand = smoothstep(0.86, 0.98, bestT) + coast * 0.6;
    float pulse = st > 2.5 ? 0.75 + 0.25 * sin(Time * 4.0) : (st > 1.5 ? 0.85 : 0.55);
    tinted += (locked ? float3(0.25, 0.32, 0.42) : float3(0.75, 0.90, 1.00)) * saturate(rimBand) * pulse * 0.6;
    col = lerp(col, tinted, inRegion);
}
col *= 0.45 + 0.55 * z;
col += float3(0.40, 0.55, 0.80) * grid * 0.16;
// Scan lines drifting down, a holographic touch.
col += float3(0.30, 0.45, 0.70) * 0.025 * step(0.5, frac(UV.y * 180.0 - Time * 0.6));
float rim = pow(1.0 - z, 3.0);
col += float3(0.45, 0.68, 1.0) * rim * 0.7;
float edge = 1.0 - smoothstep(1.0 - aa * 1.5, 1.0, r);
return float4(col, edge * 0.94);
