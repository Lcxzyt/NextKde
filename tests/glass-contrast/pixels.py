"""Assertions against rendered glyph pixels, not QML colour properties."""
from pathlib import Path
from PIL import Image


def core_pixels(path):
    image = Image.open(path).convert("RGBA")
    pixels = [(x, y, image.getpixel((x, y))) for y in range(image.height)
              for x in range(image.width) if image.getpixel((x, y))[3] >= 220]
    assert len(pixels) > 20, f"no opaque glyph coverage: {path}"
    return image, pixels


def check_split(path, reference, reverse=False):
    image, pixels = core_pixels(path)
    margin = max(2, round(image.width * 0.05))
    for dark, selected in [
        (not reverse, [rgba for x, _, rgba in pixels if x < image.width / 2 - margin]),
        (reverse, [rgba for x, _, rgba in pixels if x > image.width / 2 + margin]),
    ]:
        assert len(selected) > 10, f"missing split glyphs: {path}"
        assert all((max(rgba[:3]) < 50 if dark else min(rgba[:3]) > 230)
                   for rgba in selected), f"wrong or grey ink on split background: {path}"
    baseline = Image.open(reference).convert("RGBA")
    assert baseline.size == image.size, (baseline.size, image.size)
    errors = [abs(a - b) for a, b in zip(image.getchannel("A").getdata(), baseline.getchannel("A").getdata())]
    assert max(errors, default=0) <= 3, f"shader changed glyph coverage / added halo: {max(errors)}"


def check_sequence(directory, prefix, allowed_flips):
    paths = sorted(Path(directory).glob(f"{prefix}-*.png"))
    assert len(paths) >= 3, f"missing rendered dynamic samples: {prefix}"
    states = []
    for path in paths:
        _, pixels = core_pixels(path)
        assert all(max(rgba[:3]) < 50 or min(rgba[:3]) > 230 for _, _, rgba in pixels), \
            f"persistent grey foreground: {path}"
        states.append(sum(rgba[0] for _, _, rgba in pixels) / len(pixels) < 128)
    flips = sum(left != right for left, right in zip(states, states[1:]))
    assert flips <= allowed_flips, f"rendered ink flickers: {prefix}, {flips} flips"
