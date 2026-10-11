#!/usr/bin/env python3
"""Writes the showcase car's engine sound: a two-second drone that loops without a seam, which the
game plays faster or slower with the engine's speed. Synthesized, so there is nobody to credit.

    python3 -I tools/assets/make_engine_sound.py
"""
import math
import os
import random
import struct
import wave

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUTPUT = os.path.join(REPO_ROOT, "games", "showcase", "sounds", "engine.wav")
SAMPLE_RATE = 44100
SECONDS = 2.0
FIRING_HZ = 62.0                  # the note of the exhaust at the sound's own speed
# (harmonic of the firing note, loudness): a low growl with a rasp on top.
HARMONICS = [(0.5, 0.55), (1, 1.0), (2, 0.6), (3, 0.42), (4, 0.3), (5, 0.2), (6, 0.16), (8, 0.1), (10, 0.07)]
LUMP_HZ = 4.0                     # the uneven beat of the cylinders
LUMP_DEPTH = 0.18
NOISE_PARTIALS = 160              # the hiss of air and mechanics: many faint tones ...
NOISE_BAND_HZ = (300.0, 3200.0)   # ... spread over this range
NOISE_LEVEL = 0.22
PEAK = 0.82                       # of full scale
FULL_SCALE = 32767
SEED = 7


def main():
    rng = random.Random(SEED)
    samples = int(SAMPLE_RATE * SECONDS)
    # Every tone fits a whole number of times in the sound, which is what makes it loop cleanly.
    loop_hz = 1.0 / SECONDS
    tones = [(round(FIRING_HZ * harmonic / loop_hz) * loop_hz, level, rng.uniform(0.0, math.tau))
             for harmonic, level in HARMONICS]
    noise = [(round(rng.uniform(*NOISE_BAND_HZ) / loop_hz) * loop_hz, rng.uniform(0.2, 1.0), rng.uniform(0.0, math.tau))
             for _ in range(NOISE_PARTIALS)]
    noise_scale = NOISE_LEVEL / math.sqrt(NOISE_PARTIALS)
    wave_form = []
    for index in range(samples):
        time = index / SAMPLE_RATE
        growl = sum(level * math.sin(math.tau * hz * time + phase) for hz, level, phase in tones)
        hiss = sum(level * math.sin(math.tau * hz * time + phase) for hz, level, phase in noise) * noise_scale
        lump = 1.0 - LUMP_DEPTH * (0.5 + 0.5 * math.sin(math.tau * LUMP_HZ * time))
        wave_form.append((growl + hiss) * lump)
    loudest = max(abs(value) for value in wave_form)
    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with wave.open(OUTPUT, "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(SAMPLE_RATE)
        out.writeframes(b"".join(struct.pack("<h", int(value / loudest * PEAK * FULL_SCALE)) for value in wave_form))
    print(f"wrote {OUTPUT}")


main()
