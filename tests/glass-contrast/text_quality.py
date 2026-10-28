"""Report pipeline differences without treating unmeasured sharpness as a pass."""
import json
from pathlib import Path
from PIL import Image, ImageChops


def report_text_quality(directory):
    directory = Path(directory)
    report = {
        "scope": "whole-item captures, synthetic map; not compositor screenshot acceptance",
        "cases": [],
        "split": "inspect split-*.png visually; scalar and adaptive colours intentionally differ",
    }
    for backdrop in ["light", "dark"]:
        with Image.open(directory / f"{backdrop}-native.png") as source:
            native = source.convert("RGB")
        # The fixture's logical output is 640 x 240. Capture at its physical DPR.
        sx, sy = native.width / 640, native.height / 240
        for mode in ["glass-direct", "layer", "adaptive", "snap-adaptive"]:
            reference_mode = "snap-native" if mode == "snap-adaptive" else "native"
            if reference_mode == "snap-native":
                with Image.open(directory / f"{backdrop}-{reference_mode}.png") as source:
                    baseline = source.convert("RGB")
            else:
                baseline = native
            with Image.open(directory / f"{backdrop}-{mode}.png") as source:
                candidate = source.convert("RGB")
            assert candidate.size == native.size, (candidate.size, native.size)
            for index in range(10):
                size, offset = [10, 11, 12, 13, 16][index // 2], [0, .5][index % 2]
                box = (round(20 * sx), round((7 + index * 22) * sy),
                       round(620 * sx), round((30 + index * 22) * sy))
                reference = baseline.crop(box)
                result = candidate.crop(box)
                difference = ImageChops.difference(reference, result)
                errors = [max(pixel) for pixel in difference.getdata()]
                background = (255, 255, 255) if backdrop == "light" else (0, 0, 0)
                # Exclude the empty margins from the denominator: otherwise
                # a wide row can hide a large change to a small glyph.
                selected = [error for left, right, error in zip(
                    reference.getdata(), result.getdata(), errors)
                    if left != background or right != background]
                assert selected, f"missing glyphs: {backdrop}/{mode}/{size}"
                report["cases"].append({
                    "backdrop": backdrop, "mode": mode, "reference": reference_mode, "font_px": size,
                    "position_offset": offset, "capture_scale": [sx, sy],
                    "max_rgb_error": max(selected),
                    "mean_rgb_error_on_ink": round(sum(selected) / len(selected), 4),
                    "changed_ink_fraction": round(sum(error > 3 for error in selected) / len(selected), 4),
                })
    destination = directory / "text-quality.json"
    report["split_cases"] = split_quality(directory)
    destination.write_text(json.dumps(report, indent=2) + "\n")
    return destination


def check_aligned_quality(report):
    """Allow isolated native hint-rounding pixels, reject broad re-sampling."""
    data = json.loads(Path(report).read_text())
    rows = [row for row in data["cases"]
            if row["mode"] == "snap-adaptive"]
    assert len(rows) == 20, f"missing aligned glyph comparisons: {len(rows)}"
    for row in rows:
        assert row["mean_rgb_error_on_ink"] <= 1.5, row
        assert row["changed_ink_fraction"] <= .01, row
    assert len(data["split_cases"]) == 10, data["split_cases"]
    for row in data["split_cases"]:
        assert row["mean_rgb_error_on_ink"] <= 1.5, row
        assert row["changed_ink_fraction"] <= .01, row


def split_quality(directory):
    """Verify both endpoint inks and coverage against captured native glyphs."""
    with Image.open(directory / "light-snap-native.png") as image:
        reference = image.convert("RGB")
    with Image.open(directory / "split-snap-adaptive.png") as image:
        split = image.convert("RGB")
    assert reference.size == split.size, (reference.size, split.size)
    sx, sy = reference.width / 640, reference.height / 240
    rows = []
    for index in range(10):
        errors = []
        for y in range(round((7 + index * 22) * sy), round((30 + index * 22) * sy)):
            for x in range(round(20 * sx), round(620 * sx)):
                if abs(x - reference.width / 2) < 2 * sx:
                    continue  # interpolation boundary is tested separately by the compositor fixture
                native = reference.getpixel((x, y))
                actual = split.getpixel((x, y))
                dark = x < reference.width / 2
                background = (255, 255, 255) if dark else (0, 0, 0)
                if native == (255, 255, 255) and actual == background:
                    continue
                # Recover grayscale glyph coverage from dark native ink on white.
                # Native red ink is round(0.06 * 255) = 15.
                alpha = (255 - native[0]) / 240
                expected = native if dark else (round(255 * alpha),) * 3
                errors.append(max(abs(a - b) for a, b in zip(actual, expected)))
        assert errors, f"missing split glyphs in row {index}"
        rows.append({"font_px": [10, 11, 12, 13, 16][index // 2],
                     "position_offset": [0, .5][index % 2],
                     "mean_rgb_error_on_ink": round(sum(errors) / len(errors), 4),
                     "changed_ink_fraction": round(sum(error > 3 for error in errors) / len(errors), 4)})
    return rows
