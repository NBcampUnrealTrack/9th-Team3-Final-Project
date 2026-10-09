"""Generates the menu textures (globe land map, title logo, left-edge gradient).

Usage: python make_menu_textures.py <IBM Plex Sans KR font dir> [output dir, default Tools/UI/Textures]

The PNGs in Tools/UI/Textures are the import sources of /Game/Knight/UI/Menu/Textures; reimport after regenerating.

T_UI_GlobeLand.png   equirectangular 2048x1024. R = land (soft edge), G = coastline band, B = terrain detail.
T_UI_Logo.png        "PROJECT KNIGHT" with a blue glow, 2x resolution, transparent background.
T_UI_GradientLeft.png  white with alpha fading left to right, tinted in UMG.
T_UI_LoadingMark.png   placeholder emblem for the loading screen: tall four-point star through a ring, white on transparent.
T_UI_PartFrame.png     lock-on part frame (/Game/Knight/UI/HUD/Textures): hexagon outline with three inward triangles,
                       white on transparent, tinted in UMG by the part condition. Shown at 104 x 104.
T_UI_StudioLogo.png    team logo (Transparent Dragon) for the startup intro, final colors, 1024x1024 with mipmaps.
T_UI_UnrealLogo.png    "Unreal Engine icon - white.png" (Epic branding download) scaled to 512. Do not recolor or reshape it.

Continents are random but seeded, and pushed up under the stage-select sample regions so markers sit on land.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

SEED = 7
# Sample region positions on the globe (longitude, latitude), same as DA_StageCatalog.
REGIONS = [(24.0, 28.0), (-48.0, 12.0), (82.0, -14.0), (-8.0, -36.0)]


def unit_vectors(width, height):
    lon = (np.arange(width) + 0.5) / width * 2.0 * math.pi - math.pi
    lat = math.pi / 2.0 - (np.arange(height) + 0.5) / height * math.pi
    lon, lat = np.meshgrid(lon, lat)
    # Same axes as the material: x = cos(lat) sin(lon), y = sin(lat), z = cos(lat) cos(lon).
    return np.stack([np.cos(lat) * np.sin(lon), np.sin(lat), np.cos(lat) * np.cos(lon)], axis=-1)


def wave_noise(p, rng, count, freq_lo, freq_hi, falloff=1.0):
    """Sum of plane waves in random 3D directions: smooth, seamless on the sphere. Amplitude = freq ** -falloff."""
    total = np.zeros(p.shape[:2])
    for _ in range(count):
        direction = rng.normal(size=3)
        direction /= np.linalg.norm(direction)
        freq = rng.uniform(freq_lo, freq_hi)
        total += np.sin(p @ direction * freq + rng.uniform(0, 2 * math.pi)) * freq ** -falloff
    return total


def make_land(out_dir):
    width, height = 2048, 1024
    rng = np.random.default_rng(SEED)
    p = unit_vectors(width, height)
    # Domain warp gives ragged, organic coastlines instead of blobs.
    warp = np.stack([wave_noise(p, rng, 16, 2.0, 9.0) for _ in range(3)], axis=-1) * 0.5
    q = p + warp
    field = wave_noise(q, rng, 90, 2.0, 22.0, falloff=0.75)
    for lon, lat in REGIONS:
        lon_r, lat_r = math.radians(lon), math.radians(lat)
        centre = np.array([math.cos(lat_r) * math.sin(lon_r), math.sin(lat_r), math.cos(lat_r) * math.cos(lon_r)])
        field += 0.6 * np.exp(-((1.0 - p @ centre) / 0.02))
    land = (field > np.quantile(field, 0.6)).astype(np.float32)

    land_img = Image.fromarray((land * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.5))
    soft = np.asarray(land_img).astype(np.float32) / 255.0
    band = np.asarray(Image.fromarray((land * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3.0))).astype(np.float32) / 255.0
    coast = np.clip(1.0 - np.abs(band - 0.5) * 3.0, 0.0, 1.0)
    detail = wave_noise(p, rng, 60, 20.0, 90.0)
    detail = (detail - detail.min()) / (detail.max() - detail.min())

    rgb = np.stack([soft, coast, detail], axis=-1)
    Image.fromarray((rgb * 255).astype(np.uint8), "RGB").save(os.path.join(out_dir, "T_UI_GlobeLand.png"))
    print("land fraction %.2f" % land.mean())


def tracked_width(draw, text, font, tracking):
    return sum(draw.textlength(ch, font=font) for ch in text) + tracking * (len(text) - 1)


def draw_tracked(draw, x, baseline, text, font, tracking, fill):
    for ch in text:
        draw.text((x, baseline), ch, font=font, fill=fill, anchor="ls")
        x += draw.textlength(ch, font=font) + tracking


def make_logo(font_dir, out_dir):
    light = ImageFont.truetype(os.path.join(font_dir, "IBMPlexSansKR-Light.ttf"), 92)
    bold = ImageFont.truetype(os.path.join(font_dir, "IBMPlexSansKR-SemiBold.ttf"), 168)
    probe = ImageDraw.Draw(Image.new("L", (1, 1)))
    first, second = "PROJECT", "KNIGHT"
    track_first, track_second, gap = 30, 34, 54
    w1 = tracked_width(probe, first, light, track_first)
    w2 = tracked_width(probe, second, bold, track_second)
    pad = 60
    width = int(pad * 2 + w1 + gap + w2)
    height = 300
    baseline = 210

    text = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(text)
    draw_tracked(draw, pad, baseline, first, light, track_first, 255)
    draw_tracked(draw, pad + w1 + gap, baseline, second, bold, track_second, 255)

    # Glow: wide soft halo plus a tight bright one, in the style guide's frost blue.
    glow_wide = np.asarray(text.filter(ImageFilter.GaussianBlur(26))).astype(np.float32) / 255.0
    glow_tight = np.asarray(text.filter(ImageFilter.GaussianBlur(7))).astype(np.float32) / 255.0
    core = np.asarray(text).astype(np.float32) / 255.0
    glow_color = np.array([0.37, 0.62, 1.0])
    core_color = np.array([0.90, 0.95, 1.0])
    glow_alpha = np.clip(glow_wide * 0.9 + glow_tight * 0.8, 0.0, 1.0)
    alpha = np.clip(core + glow_alpha * (1.0 - core), 0.0, 1.0)
    rgb = (core[..., None] * core_color + (glow_alpha * (1.0 - core))[..., None] * glow_color) / np.maximum(alpha[..., None], 1e-4)
    rgba = np.concatenate([np.clip(rgb, 0, 1), alpha[..., None]], axis=-1)
    Image.fromarray((rgba * 255).astype(np.uint8), "RGBA").save(os.path.join(out_dir, "T_UI_Logo.png"))
    print("logo %dx%d" % (width, height))


def make_gradient(out_dir):
    width = 512
    x = np.linspace(0.0, 1.0, width)
    alpha = 1.0 - (x * x * (3.0 - 2.0 * x))
    rgba = np.zeros((8, width, 4), dtype=np.float32)
    rgba[..., :3] = 1.0
    rgba[..., 3] = alpha
    Image.fromarray((rgba * 255).astype(np.uint8), "RGBA").save(os.path.join(out_dir, "T_UI_GradientLeft.png"))


def make_loading_mark(out_dir):
    # Drawn 4x larger and scaled down for smooth edges.
    scale = 4
    width, height = 192, 256
    cx, cy = width * scale / 2, height * scale / 2
    s = lambda v: v * scale

    def star(reach_y, reach_x, waist):
        return [(cx, cy - s(reach_y)), (cx + s(waist), cy - s(waist)), (cx + s(reach_x), cy), (cx + s(waist), cy + s(waist)),
                (cx, cy + s(reach_y)), (cx - s(waist), cy + s(waist)), (cx - s(reach_x), cy), (cx - s(waist), cy - s(waist))]

    img = Image.new("L", (width * scale, height * scale), 0)
    draw = ImageDraw.Draw(img)
    ring_r, ring_w = 54, 5
    draw.ellipse([cx - s(ring_r), cy - s(ring_r), cx + s(ring_r), cy + s(ring_r)], outline=255, width=s(ring_w))
    # Clear a margin around the star so its points cross the ring cleanly.
    draw.polygon(star(124, 70, 22), fill=0)
    draw.polygon(star(118, 62, 14), fill=255)
    # Small diamond cut out of the centre.
    draw.polygon([(cx, cy - s(16)), (cx + s(9), cy), (cx, cy + s(16)), (cx - s(9), cy)], fill=0)
    alpha = img.resize((width, height), Image.LANCZOS)
    rgba = Image.new("RGBA", (width, height), (255, 255, 255, 0))
    rgba.putalpha(alpha)
    rgba.save(os.path.join(out_dir, "T_UI_LoadingMark.png"))


def make_part_frame(out_dir):
    # 2x the on-screen size (104) for sharp edges at 4K, drawn 4x larger again and scaled down.
    size, scale = 208, 4
    c = size * scale / 2
    s = lambda v: v * scale * 2  # in on-screen units
    img = Image.new("L", (size * scale, size * scale), 0)
    draw = ImageDraw.Draw(img)
    # Pointy-top hexagon, radius 46: same shape as the approved mockup.
    hexagon = [(c + s(46) * math.cos(math.radians(a)), c + s(46) * math.sin(math.radians(a))) for a in range(-90, 270, 60)]
    draw.line(hexagon + [hexagon[0]], fill=255, width=int(s(2)), joint="curve")
    # Solid triangles on the top, lower right and lower left corners, pointing at the centre.
    for x, y in ((0, -46), (40, 23), (-40, 23)):
        inward = (-x / 46.0, -y / 46.0)
        side = (-inward[1], inward[0])
        tip = (c + s(x + inward[0] * 16), c + s(y + inward[1] * 16))
        base = (c + s(x), c + s(y))
        left = (base[0] + s(side[0] * 9), base[1] + s(side[1] * 9))
        right = (base[0] - s(side[0] * 9), base[1] - s(side[1] * 9))
        draw.polygon([tip, left, right], fill=255)
    alpha = img.resize((size, size), Image.LANCZOS)
    rgba = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    rgba.putalpha(alpha)
    rgba.save(os.path.join(out_dir, "T_UI_PartFrame.png"))


def make_studio_logo(font_dir, out_dir):
    # Team logo for the startup intro: a dragon coiled into an open ring around an eye, "TRANSPARENT / DRAGON" below.
    # Coordinates are in layout units (1 unit = 4 px), drawn 3x larger again and scaled down.
    unit, ss = 4, 3
    width, height = 300, 270
    k = unit * ss
    white, frost, orange, back = (244, 248, 252, 255), (168, 200, 240, 255), (255, 122, 26, 255), (0, 0, 0, 0)
    img = Image.new("RGBA", (width * k, height * k), back)
    draw = ImageDraw.Draw(img)
    cx, cy = 150, 92
    p = lambda x, y: ((cx + x) * k, (cy + y) * k)

    # Body: open ring from the tail (upper right) round to the head; the gap is where the head meets the tail.
    r, w = 62, 6
    draw.arc([p(-r - w / 2, -r - w / 2), p(r + w / 2, r + w / 2)], start=13.1, end=308.4, fill=white, width=w * k)
    # Spine: small notches cut along the body.
    for a in range(20, 305, 12):
        rad = math.radians(a)
        x, y = math.cos(rad) * r, math.sin(rad) * r
        draw.ellipse([p(x - 1.1, y - 1.1), p(x + 1.1, y + 1.1)], fill=back)
    draw.polygon([p(60, 14), p(82, -12), p(70, 18)], fill=white)
    jaw = Image.new("RGBA", img.size, back)
    ImageDraw.Draw(jaw).polygon([p(60, 14), p(90, 4), p(72, 22)], fill=white[:3] + (140,))
    img.alpha_composite(jaw)
    draw.polygon([p(42, -50), p(50, -72), p(36, -46)], fill=frost)
    # Inner ring and the eye.
    inner = Image.new("RGBA", img.size, back)
    ImageDraw.Draw(inner).ellipse([p(-36, -36), p(36, 36)], outline=frost[:3] + (80,), width=k)
    img.alpha_composite(inner)
    draw.line([p(0, -20), p(7, 0), p(0, 20), p(-7, 0), p(0, -20)], fill=orange, width=int(2 * k), joint="curve")

    # Wordmark.
    bold = ImageFont.truetype(os.path.join(font_dir, "IBMPlexSansKR-SemiBold.ttf"), 26 * k)
    light = ImageFont.truetype(os.path.join(font_dir, "IBMPlexSansKR-Regular.ttf"), 20 * k)

    def centred(text, font, tracking, baseline, fill):
        x = (width * k - tracked_width(draw, text, font, tracking * k)) / 2
        draw_tracked(draw, x, baseline * k, text, font, tracking * k, fill)

    centred("TRANSPARENT", bold, 9, 204, white)
    divider_y = 222
    draw.line([p(-80, divider_y - cy), p(-18, divider_y - cy)], fill=frost[:3] + (128,), width=k)
    draw.line([p(18, divider_y - cy), p(80, divider_y - cy)], fill=frost[:3] + (128,), width=k)
    draw.polygon([p(0, divider_y - cy - 4), p(4, divider_y - cy), p(0, divider_y - cy + 4), p(-4, divider_y - cy)], fill=frost)
    centred("DRAGON", light, 16, 254, frost)

    # Power-of-two canvas so the texture gets mipmaps; the logo grows slowly in the intro and shimmers without them.
    logo = img.resize((960, 864), Image.LANCZOS)
    canvas = Image.new("RGBA", (1024, 1024), back)
    canvas.paste(logo, ((1024 - 960) // 2, (1024 - 864) // 2))
    canvas.save(os.path.join(out_dir, "T_UI_StudioLogo.png"))


def make_unreal_logo(out_dir):
    # Official white Unreal Engine mark (from Epic's branding page), only scaled down to the size the intro shows.
    src = os.path.join(out_dir, "Unreal Engine icon - white.png")
    Image.open(src).convert("RGBA").resize((512, 512), Image.LANCZOS).save(os.path.join(out_dir, "T_UI_UnrealLogo.png"))


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    font_dir = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) == 3 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "Textures")
    os.makedirs(out_dir, exist_ok=True)
    make_land(out_dir)
    make_logo(font_dir, out_dir)
    make_gradient(out_dir)
    make_loading_mark(out_dir)
    make_part_frame(out_dir)
    make_studio_logo(font_dir, out_dir)
    make_unreal_logo(out_dir)
