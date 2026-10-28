# Appearance architecture

[简体中文](AppearanceArchitecture.zh-CN.md)

For AI contributors. Read [project boundaries](ProjectArchitecture.md) and
[window rendering](WindowAppearance.md) before changing presentation or compositor code.
This is the current design, not a runtime acceptance report.

## Ownership and source entry points

| Owner | Contract |
| --- | --- |
| `shell/desktop/modules/common/AppearanceConfigService.qml` | Validated Shell appearance settings, migrations and snapshots |
| `AppearanceTokens.qml` in the same directory | Derived visual tokens, version 9; consumers use tokens rather than duplicate style branches |
| `IconAppearanceService.qml`, `BundledIcons.qml`, `BundledIcon.qml`, `SystemIconResolver.qml` | Icon mode/persistence, named bundled assets and centralized system-theme fallback |
| `shell/desktop/DesktopEnvironment.qml` | `appearance-settings` IPC endpoint |
| `apps/settings/` | Separate process; edits through IPC, never imports Shell internals |
| `shared/qml/colorize/` | Portable pure-JS palette/HCT algorithms; no dependency on Quickshell |
| `shared/qml/controls/` and Shell common components | Reusable surfaces/controls that consume explicit theme inputs |

`AppearanceConfigService` owns Shell configuration; `AppearanceTokens` derives colors, geometry,
opacity and motion. Shell style (`windows12` / `macos` / `material`), widget style, glass preset
(`liquid` / `soft` / `frosted`), bar integration/layout/visibility and Dock configuration are
separate choices. Do not reset Dock pins, sizes or independent state when changing style.
Standalone app preferences remain separate; see [project context](../PROJECT_CONTEXT.md).

## Persistence and IPC

`Quickshell.stateDir/appearance/config.json` is currently written with schema **30**; source
`serialize()`/load migrations are authoritative for the full field set. Do not copy a partial
JSON example over a user's whole file. Keep old-version migration, per-preset optical values,
validation and range clamping. Unsupported style values must not become unvalidated runtime state.
Icon settings use separate `appearance/icon-appearance.json` (schema 1), including the one-time
legacy Dock migration. Saving is debounced through `JsonConfigStore` → platform `state.write`
→ `QSaveFile`, rather than a QML shell command.

Settings calls `appearance-settings` snapshot/update operations and receives a complete snapshot.
Check the endpoint when adding a setting; expose bounded values, preserve compatibility aliases
and handle unavailable Shell/platform services. Glass style changes apply the complete preset;
Material has its own blur policy. A glass-strength reset does not reset every user preference.

## Adaptive glass foreground

Glass protocol v10 adds `backdrop_map`: the same sample, taken over the whole visible
shape at about one cell per 8 logical px (at most 64×48, 3072 cells), sent as one byte per
cell, top row first, and smoothed with the scalar's 75 ms time constant. The whole shape
must be repainted in the current frame, otherwise the read is deferred. The native bridge
exposes it as `backdropMapSource` (an `image://kos-backdrop/...` texture). The brightness
map remains a measurement, including for v10 clients and `backdropAt` diagnostics.
The native `BackdropInkPolicy` derives a separate binary `backdropInkMapSource`: encoded
brightness initializes dark ink at about 0.46, uses 0.42/0.50 hysteresis and 110 ms candidate
confirmation, and postpones rapid reversals until 450 ms of stability. A 25 ms timer runs
only while a candidate is pending, so a one-off update can finish without another protocol
event. Grid changes reset cell history. KWin completes per-cell smoothing even when the
central mean stays unchanged.

`GlassText`, `BundledIcon`, `WifiSignalIcon` and Control Center glyphs resolve their nearest
`glassContrastSource`. A uniform ink map returns an endpoint foreground colour; text that
already has that colour keeps direct native rendering with vertical font hinting, without
a layer or readability backdrop. Mixed maps apply `AdaptiveInkEffect` to neutral ink. Its
declared `source` receives the layer's glyph texture; the shader preserves coverage and
thresholds the interpolated binary map, avoiding persistent grey ink. It tracks relative
geometry on visible frames without ancestor signal handlers, and writes uniforms only when
coordinates change. Coloured, active-state and artwork pixels keep their own colour.
For stationary native `GlassText`, `pixelAlignedInk` snaps the layered painting
origin to the nearest physical pixel using the window DPR. It applies at most
half a physical pixel of translation without changing anchors or layout metrics.
`AdaptiveInkEffect` refreshes that correction before mapping the glyph to its
backdrop, so the ink boundary follows the corrected position. Scale/rotation,
distance-field rendering and direct uniform-map text bypass the correction.
This avoids re-sampling a hinted small glyph from a fractional layer origin;
it does not restore LCD subpixel coverage after conversion to glyph alpha.
Older native bridges without an ink map use whole-panel feedback; protocol v9 has only the
scalar and v8 keeps theme ink. QML shader source and `.qsb` must be rebuilt together.

Protocol v11 adds `set_contrast_only`: a declared foreground probe samples its own region
without painting blur, tint, scrim, rim or noise. `BackdropInkProbe` gives the floating
notification header its own map, independent of the first notification card and scrolling.
The client publishes no probe below v11; an older native module also leaves it inactive.
The header then uses theme ink rather than sampling an unrelated card. Footer glass
buttons resolve their own glass as the contrast source.

Glass protocol v9 adds opt-in `set_contrast_feedback` and `backdrop_luminance` feedback.
The compositor samples each visible declared panel after its glass finish and before client
foreground; the sample is restricted to the client's bounds and the currently repainted
shape. The scalar is the mean of its central half. Fenced PBO reads are bounded and do
not wait for GPU completion. Sampling
uses 150 ms intervals at rest and 50 ms during detected changes; a time-based 75 ms
low-pass filter smooths luminance. A skipped update receives a bounded delayed repaint so one-off background changes
are not lost. Old protocols keep the theme ink. Panel foreground changes use 0.45/0.55
hysteresis, 110 ms confirmation and a 90 ms ink tween. Three threshold crossings within
600 ms postpone further switches until a 450 ms stable interval. Without a stable ink map,
light text retains a faint readability backdrop and light monochrome icons a low-opacity
dark shadow, with no white halo on dark ink. The launcher, Control
Center and notifications opt in. Monochrome symbols
resolve their nearest `glassContrastSource`; artwork and active/danger state colors stay
explicit. Only the scalar fallback uses the central average for a mixed panel.
`tests/glass-contrast/` checks policy logic, final glyph RGB/alpha, sampled dynamic frames
and independent header sampling. The CI inventory includes four logic cases and six
synthetic glyph-composition cases (1/1.5/2 scale, fixed/natural width); the latter use the
candidate shader/native module without a Glass effect. Real feedback tests additionally
require the candidate Glass plugin; registration is not runtime acceptance. Multi-output, HDR, real fractional
scaling and performance still require separate measurement. New compositor binaries require
a fresh login; never hot-reload them.

## Color and contrast invariants

Use `AppearanceTokens.isDarkTheme` and semantic content tokens, especially `content.glassInk`.
Do not pick foreground ink directly from the system dark-mode flag. When
`glassFollowsAppearanceMode` is false, dark glass can remain on a light desktop and still needs
light ink. Separate material fill opacity, scrim and background blur; zero tint/fill opacity does not
imply zero blur. Protocol material opacity fades the complete finish, including blur. See protocol ownership in [window appearance](WindowAppearance.md).

Material palettes are generated locally in `shared/qml/colorize/`: `monet` uses CAM16/HCT,
while `chinese` and `japanese` select traditional-color data with the shared contrast policy.
Keep sRGB byte rounding, HCT gamut handling and contrast regression tests together; do not replace
the algorithm with external `matugen` processes or approximate Lab conversion. Extract wallpaper
colors from the original image, not Plasma's black/white placeholder. Resolve Plasma's wallpaper
configuration through the current `plasmashellrc` ShellPackage, not a hardcoded old appletsrc.

The source colorize directory is not a compiled `Kos.Ui` module. Keep source aliases, `qmldir`,
resource deployment and relative imports working in both Shell and Settings. Portable app/shared
code must not import Quickshell-specific adapters. Material card surfaces/flower blur geometry
use shared components; do not scatter individual style checks throughout every control.

## Icons, geometry and motion

Register Shell-owned imagery by stable name in `BundledIcons`; render through `BundledIcon` or
its registry source. Do not depend on workstation absolute paths or font glyphs. Application
icons, media art and file thumbnails follow their own contracts. System-theme exceptions such as
the Dock trash icon resolve through `SystemIconResolver`, with centralized candidates and bundled fallback.

Tray identity resolution lives in `modules/bar/TrayIdentity.mjs`: explicit tooltip/title
labels and app-specific IDs win over daemon fallback names keyed by a shared generated ID.
Two anonymous entries sharing an ID remain unresolved; topology changes invalidate cached
names and stale in-flight replies. Monochrome templates use exact app aliases, preserve live
attention/status artwork, and fall back to original artwork when a replacement cannot load.

Use token geometry and motion values consistently across Dock, bar and widgets. Dock window-animation
targets use stable layout slot centers and static icon size in compositor-global coordinates;
hover magnification must not move a minimize target. Keep desktop file spacing in
[DesktopIcons](DesktopIcons.md). Material surfaces and continuous compositor corners have different
rendering owners; a visual style change must not duplicate clipping or shadows.

## Shell text rendering

`common/ShellText.qml` uses platform-native rendering for stationary small text,
matching the bar's native labels. `GlassText`, context-menu rows, launcher labels
and Dock name tooltips share this policy. Notification/history, Control Center,
Quick Search, desktop widgets and Dock information popups inherit it through
`GlassText`. Font families, sizes, weights and system hinting preferences remain
owned by their existing callers/platform settings.

`ShellText` tracks its own and ancestor Item scale/rotation and uses distance-field
rendering while transformed; it returns to native rendering at rest. A host using
an explicit Scale/Rotation transform list must set `scalableRendering` while that
transform is active. Launcher search/rename inputs follow their motion wrappers
and use native rendering at rest too.

Context menus mask the full content during capsule reveal and for overflowing
lists, whose partially scrolled rows can cross the corners. Settled menus that
fit draw their inset rows directly into the window. `LiquidGlassPanel.maskContent: false`
keeps the QML body independently masked, so Material/fallback corners remain
shaped without rasterizing text into the same offscreen texture.

## Verification and review

Run tests only in VM/CI per [Development](Development.md). Cover migrations, snapshot/IPC values,
light/dark/system themes, dark glass on a light desktop, all glass/material/color schemes,
bar/Dock integration, source/installed module paths, icon fallback and reduced motion.
Review fullscreen/maximized/mixed-scale window behavior separately in [WindowAppearance](WindowAppearance.md).
A passed offscreen token test is not proof of live blur, GPU cost or multi-display behavior.
For text changes, compare Chinese/Latin labels and inputs at 100%, 125%, 150%
and 200% output scales, including moving between outputs. Exercise menu reveal,
submenus/scrolling, launcher search/folders/edit/drag and open/close reversal;
check both settled text and moving text in glass and Material themes.
