"""Synthesizes the temporary UI sounds (S_UI_*.wav) described in the UI style guide.

These are placeholders until the final AI-generated sounds pass the license check. Keep the file names:
the imported SoundWave assets are replaced in place by reimporting a new WAV with the same name.

Usage: python make_temp_ui_sounds.py <output_dir>
Requires numpy and scipy.
"""
import sys
import wave
from pathlib import Path

import numpy as np
from scipy.signal import butter, sosfilt

SR = 48000
rng = np.random.default_rng(7)


def t_axis(seconds):
    return np.arange(int(SR * seconds)) / SR


def env(seconds, tau, attack=0.002):
    t = t_axis(seconds)
    a = np.clip(t / attack, 0.0, 1.0)
    return a * np.exp(-t / tau)


def partials(freqs, seconds, tau, amps=None, detune=1.0):
    t = t_axis(seconds)
    amps = amps or [1.0 / (i + 1) for i in range(len(freqs))]
    out = sum(a * np.sin(2 * np.pi * f * detune * t) for f, a in zip(freqs, amps))
    return out * env(seconds, tau)


def band_noise(seconds, lo, hi, tau):
    sos = butter(2, [lo, hi], btype="band", fs=SR, output="sos")
    return sosfilt(sos, rng.standard_normal(int(SR * seconds))) * env(seconds, tau, attack=0.0005)


def lowpass(x, cutoff):
    return sosfilt(butter(2, cutoff, btype="low", fs=SR, output="sos"), x)


def place(total, *parts):
    """parts: (start_seconds, signal)"""
    out = np.zeros(int(SR * total))
    for start, sig in parts:
        i = int(SR * start)
        n = min(len(sig), len(out) - i)
        out[i:i + n] += sig[:n]
    return out


def tail(x, seconds=0.0):
    # Short fade so nothing clicks at the end.
    if seconds:
        x = np.concatenate([x, np.zeros(int(SR * seconds))])
    fade = min(len(x), int(SR * 0.004))
    x[-fade:] *= np.linspace(1.0, 0.0, fade)
    return x


def normalize(x, peak_db):
    return x / (np.max(np.abs(x)) + 1e-9) * (10 ** (peak_db / 20))


def write(path, x):
    pcm = (np.clip(x, -1.0, 1.0) * 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def hover():
    x = place(0.05,
              (0.0, band_noise(0.01, 4000, 9000, 0.0015) * 0.6),
              (0.0, partials([3200, 5150], 0.05, 0.008)))
    return normalize(tail(x), -10)


def confirm():
    latch = lambda f: partials([f, f * 1.7, f * 2.68], 0.09, 0.03)
    x = place(0.15,
              (0.0, band_noise(0.01, 2000, 7000, 0.002)),
              (0.0, latch(1100) * 0.7),
              (0.045, band_noise(0.01, 3000, 8000, 0.002) * 0.8),
              (0.045, latch(1650) * 0.6),
              (0.0, partials([90], 0.12, 0.045) * 0.9))
    return normalize(tail(x), -3)


def back():
    x = place(0.12,
              (0.0, band_noise(0.01, 1500, 5000, 0.002) * 0.8),
              (0.0, partials([760, 1290, 2040], 0.08, 0.025) * 0.7),
              (0.035, partials([560, 950], 0.08, 0.025) * 0.6),
              (0.0, partials([70], 0.1, 0.04) * 0.8))
    return normalize(tail(x), -4)


def error():
    def buzz():
        t = t_axis(0.075)
        sq = np.sign(np.sin(2 * np.pi * 165 * t)) * 0.6 + np.sin(2 * np.pi * 330 * t) * 0.4
        e = np.clip(t / 0.003, 0, 1) * np.clip((0.075 - t) / 0.01, 0, 1)
        return lowpass(sq * e, 1800)
    x = place(0.19, (0.0, buzz()), (0.1, buzz()))
    return normalize(tail(x), -5)


def tab():
    n = int(SR * 0.09)
    t = np.arange(n) / SR
    noise = rng.standard_normal(n)
    # Sweep a bandpass upward by filtering in short blocks: a quick metallic slide.
    out = np.zeros(n)
    block = 480
    for i in range(0, n, block):
        c = 1500 + 5000 * (i / n)
        sos = butter(2, [c * 0.7, c * 1.3], btype="band", fs=SR, output="sos")
        out[i:i + block] = sosfilt(sos, noise[i:i + block])
    out *= np.exp(-t / 0.03) * np.clip(t / 0.004, 0, 1)
    x = place(0.09, (0.0, out * 0.7), (0.06, partials([2600, 4100], 0.03, 0.006) * 0.6))
    return normalize(tail(x), -6)


def lock_on():
    def beep(f):
        t = t_axis(0.055)
        s = np.sin(2 * np.pi * f * t) + 0.25 * np.sin(2 * np.pi * f * 3 * t)
        return s * np.clip(t / 0.003, 0, 1) * np.clip((0.055 - t) / 0.012, 0, 1)
    x = place(0.19, (0.0, beep(1500)), (0.075, beep(2000)))
    return normalize(tail(x), -6)


def slider():
    x = place(0.03, (0.0, partials([4200, 6500], 0.03, 0.004)), (0.0, band_noise(0.006, 5000, 10000, 0.001) * 0.4))
    return normalize(tail(x), -12)


def part_alert():
    # Locked part turned critical or broke: a dry crack over a falling two-note warning.
    def tone(f):
        t = t_axis(0.09)
        s = np.sign(np.sin(2 * np.pi * f * t)) * 0.5 + np.sin(2 * np.pi * f * t) * 0.5
        return lowpass(s * np.clip(t / 0.003, 0, 1) * np.clip((0.09 - t) / 0.02, 0, 1), 2400)
    crack = band_noise(0.04, 1200, 6000, 0.008)
    x = place(0.26, (0.0, crack * 1.2), (0.0, tone(880) * 0.6), (0.11, tone(587) * 0.6), (0.0, partials([75], 0.2, 0.05) * 0.8))
    return normalize(tail(x), -4)


def deploy():
    total = 0.95
    clank = partials([420, 713, 1130, 1790, 2610], 0.6, 0.18, amps=[1, .8, .6, .45, .3])
    thump = partials([55, 110], 0.5, 0.14, amps=[1, .4])
    hit = band_noise(0.02, 800, 6000, 0.004)
    dry = place(total, (0.0, hit), (0.0, clank * 0.6), (0.0, thump * 1.2))
    # Cheap reverb: a few decaying feedback-free echoes of a lowpassed copy.
    wet = np.zeros_like(dry)
    soft = lowpass(dry, 3000)
    for d, g in [(0.031, .35), (0.047, .3), (0.071, .25), (0.113, .2), (0.167, .14), (0.241, .09)]:
        i = int(SR * d)
        wet[i:] += soft[:-i] * g
    return normalize(tail(dry + wet), -2)


SOUNDS = {
    "S_UI_Hover": hover,
    "S_UI_Confirm": confirm,
    "S_UI_Back": back,
    "S_UI_Error": error,
    "S_UI_Tab": tab,
    "S_UI_LockOn": lock_on,
    "S_UI_Slider": slider,
    "S_UI_Deploy": deploy,
    "S_UI_PartAlert": part_alert,
}

if __name__ == "__main__":
    out_dir = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, make in SOUNDS.items():
        x = make()
        write(out_dir / f"{name}.wav", x)
        print(f"{name}.wav  {len(x) / SR * 1000:.0f} ms")
