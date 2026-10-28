# Glass foreground contrast

Build Glass, `kos_surface_shape` and the QML shaders in the VM/CI. This test loads
the candidate effect through a temporary
`QT_PLUGIN_PATH`, uses a private virtual KWin and D-Bus, and never reloads the
logged-in desktop compositor. It swaps real black/white backgrounds behind two
glass panels and checks their independent ink, symbols and preserved active ink.

```sh
bash shell/desktop/shaders/compile.sh
python3 tests/glass-contrast/run.py --plugin /absolute/build/src/glass.so --qml /absolute/build/qml
```

The Python runner requires Pillow for final pixel assertions. Run modes separately:

- `--pixel`: assert that opaque glyph pixels on opposite sides have opposite ink,
  swap the two backgrounds without changing the panel mean and assert again,
  and compare the complete glyph alpha against native text without a shader. This
  fails on grey ink, missing glyph input, changed coverage and added halos.
- `--dynamic`: slow ramps, grey noise and rapid black/white flashes; capture the
  rendered text repeatedly and assert endpoint ink and bounded visible flips.
  A settled uniform map must leave text's offscreen layer disabled.
- `--probe`: a header above a horizontal window edge must choose ink independently
  of the card below it. `--probe --max-version 10` verifies that an older server
  receives no unsupported foreground-only shape.
- `--scalar` and `--legacy`: protocol v9 whole-panel feedback and v8 theme fallback.
- `--pixel --max-version 10`: retain v10 brightness-map compatibility.

`--scale 1.5` / `--scale 2` additionally exercise forced Qt scaling. This is not
multi-output or real fractional-output acceptance. All captures are isolated
test text, never screenshots of the logged-in desktop.

The logic-only `kos-glass.contrast-filter`, `kos-glass.contrast-policy` and
`kos-glass.ink-policy` cases cover temporal filtering, scalar timing and per-cell
ink confirmation, noise, rapid reversal, resize and unchanged-mean local changes.

The default paths match `kosctl`'s build products. To register it with the root
CTest build, configure `-DKOS_GLASS_TEST_PLUGIN=/absolute/build/src/glass.so` and
run `ctest --test-dir <build> -R '^kos-glass\.(foreground-contrast|ink-policy)' --output-on-failure`.
The default, pixel, dynamic, probe, probe-v10, scalar and legacy compositor modes
are registered separately; a successful QML marker alone cannot pass pixel modes.

Brightness sampling covers each shape at about one cell per eight logical pixels.
The native bridge derives a stable binary ink map; uniform maps use direct native
text, while mixed maps use a foreground shader. Protocol v11 header probes sample
without painting any material. Older servers disable probes instead of sampling
an unrelated card. Keep candidate plugin, native module and shader builds together.
Multi-output/HDR and fractional-scale runtime acceptance are separate checks.
Logs go to ignored `tmp/glass-contrast/`; no desktop screenshot or
personal configuration is written by the test. Production plugins require a
fresh login after installation.

## Small-text pipeline diagnosis

In the VM, run this exploratory mode separately from the feedback assertions:

```sh
python3 tests/glass-contrast/run.py --text-quality --plugin /absolute/build/src/glass.so --qml /absolute/build/qml
```

`text-quality.qml` uses the production `ShellText`, `GlassText` and
`AdaptiveInkEffect`, with a synthetic binary map instead of compositor sampling.
It captures the same full 640×240 scene in six successive states, keeping the
text, default font family, Medium weight, layout and backdrop unchanged:

1. Native `ShellText`, without a layer.
2. `GlassText`, with its layer disabled.
3. `GlassText` through the actual ink shader with adaptation disabled.
4. `GlassText` through the same shader with adaptation enabled.
5. Native reference translated to the nearest physical pixel origin.
6. Production `GlassText` with `pixelAlignedInk` enabled, compared to state 5.

States 1–4 disable the correction to retain the original diagnostic baseline.
`--natural-width` uses each label's centered implicit width, matching launcher
labels rather than a fixed-width text block. Qt applies the capture DPR itself;
do not multiply the requested grab size by DPR again.

Rows use 10, 11, 12, 13 and 16 logical pixels, each at integer and half-pixel
positions. The 12px Medium row matches a centered launcher label
profile. The visible scene is captured at the attached screen's device pixel
ratio, rather than grabbing an isolated glyph before composition. Run again
with `--scale 1.5` or `--scale 2` to investigate scaling separately.

The diagnostic sets `FC_LANG=zh-cn`, matching the Shell service. By default the
isolated fontconfig has system defaults. To reproduce a desktop's hinting and
subpixel policy, pass `--fontconfig /absolute/path/to/fonts.conf`; the override
is copied into the temporary XDG config, never edited in place. Results gain
a `-fontconfig` suffix. Compare both policies before attributing a difference
to the desktop: native LCD text and a grayscale glyph-alpha layer can differ
even at integer positions. The runner uses software OpenGL, so real GPU and
desktop-compositor acceptance remains separate.

Light, dark and split backdrops produce eighteen PNGs. For uniform backgrounds,
`text_quality.py` reports RGB differences from the native reference over pixels
occupied by either glyph, including the fraction differing by more than three
channel levels. Empty margins do not dilute the measurements. Inspect the split
captures visually: their adaptive and scalar colours intentionally differ.
Uniform cases force the shader path to measure its cost even though production
would normally bypass it for a uniform map.

The output is `tmp/glass-contrast/captures-text-quality-v11-scale1/`
(suffix follows the requested protocol and scale). `text-quality.json` records
differences, not a subjective sharpness score. A successful capture marker does
not establish that the text is sharp or that adaptation caused a defect. These
are client-side full-item captures with a synthetic map; actual screen captures,
the real compositor feedback, font fallback and production ancestor masks still
need separate runtime acceptance.

For a grayscale fontconfig policy, add `--check-text` to gate the correction:
each row's mean RGB difference over glyph pixels must be at most 1.5 channel
levels, and at most 1% of its glyph pixels may differ by more than three levels.
This allows isolated native hint-rounding differences while rejecting the broad
fractional-origin sampling defect. Both uniform backdrops and the split ink
coverage are checked against captured native glyphs; the split assertion excludes
two logical pixels around the interpolation boundary. Real compositor mapping
and boundary placement remain owned by `--pixel`, not the synthetic fixture.
Run the gate at scales 1, 1.5 and 2 with fixed and natural widths.

## Hosted CI coverage

The CI inventory requires four logic cases (contrast filter, scalar policy, ink
policy and map helpers), plus six `kos-glass.text-quality-*` cases covering
1/1.5/2 scaling with fixed and natural label widths. The latter use the checked-in
grayscale fontconfig, a private software-rendered KWin, and synthetic ink maps;
they do not load a Glass effect binary. Their pixel assertions verify the actual
production glyph shader and alignment, not compositor background sampling.
Real feedback modes remain opt-in through `KOS_GLASS_TEST_PLUGIN` and need VM/CI
with the candidate Glass plugin. A passing synthetic test does not establish
real compositor sampling, GPU performance, HDR or multi-output acceptance.
