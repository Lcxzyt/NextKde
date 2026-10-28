#!/usr/bin/env python3
"""New Glass plugin + native client, only inside a private KWin and D-Bus."""
from pathlib import Path
import os
import argparse
import re
import shutil
import subprocess
import sys
import tempfile
import time

REPO = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--private-bus", action="store_true")
parser.add_argument("--legacy", action="store_true")
parser.add_argument("--dynamic", action="store_true")
parser.add_argument("--scalar", action="store_true", help="client bound at v9: whole-panel ink only")
parser.add_argument("--pixel", action="store_true", help="assert final split glyph pixels and alpha coverage")
parser.add_argument("--probe", action="store_true", help="v11 header sampling independently of its card")
parser.add_argument("--text-quality", action="store_true", help="compare six small-text rendering pipelines")
parser.add_argument("--fontconfig", type=Path, help="copy a fontconfig override into the isolated config (text diagnosis only)")
parser.add_argument("--natural-width", action="store_true", help="use centered natural label widths in text diagnosis")
parser.add_argument("--check-text", action="store_true", help="check aligned text against native (grayscale fontconfig required)")
parser.add_argument("--max-version", type=int, choices=[8, 9, 10, 11], default=11)
parser.add_argument("--scale", type=float, choices=[1.0, 1.5, 2.0], default=1.0)
parser.add_argument("--plugin", type=Path, default=REPO / ".build/kosctl/kwin-effects-glass/src/glass.so")
parser.add_argument("--qml", type=Path, default=REPO / ".build/kosctl/qml")
options = parser.parse_args()
if options.text_quality and (options.legacy or options.dynamic or options.scalar or options.pixel or options.probe):
    parser.error("--text-quality runs separately from compositor feedback modes")
if options.fontconfig and (not options.text_quality or not options.fontconfig.is_file()):
    parser.error("--fontconfig requires --text-quality and an existing override file")
if options.natural_width and not options.text_quality:
    parser.error("--natural-width requires --text-quality")
if options.check_text and (not options.text_quality or not options.fontconfig):
    parser.error("--check-text requires --text-quality and a grayscale --fontconfig override")
if options.pixel or options.dynamic:
    from pixels import check_sequence, check_split
if options.text_quality:
    from text_quality import report_text_quality, check_aligned_quality
if not options.private_bus:
    raise SystemExit(subprocess.call(["dbus-run-session", "--", sys.executable, __file__,
        "--private-bus", "--plugin", str(options.plugin), "--qml", str(options.qml),
        "--max-version", str(options.max_version), "--scale", str(options.scale)]
        + (["--legacy"] if options.legacy else []) + (["--dynamic"] if options.dynamic else [])
        + (["--scalar"] if options.scalar else []) + (["--pixel"] if options.pixel else [])
        + (["--probe"] if options.probe else [])
        + (["--text-quality"] if options.text_quality else [])
        + (["--fontconfig", str(options.fontconfig.resolve())] if options.fontconfig else [])
        + (["--natural-width"] if options.natural_width else [])
        + (["--check-text"] if options.check_text else [])))
with tempfile.TemporaryDirectory(prefix="kos-glass-contrast-") as directory:
    root = Path(directory)
    for name in ["runtime", "config", "data", "cache", "state", "plugins/kwin/effects/plugins", "shell", "captures"]:
        (root / name).mkdir(parents=True, mode=0o700)
    plugin = options.plugin
    if not options.text_quality:
        shutil.copy2(plugin, root / "plugins/kwin/effects/plugins/glass.so")
    (root / "config/kwinrc").write_text("[Plugins]\nglassEnabled=" + ("false" if options.text_quality else "true") + "\nblurEnabled=false\nblurplusEnabled=false\n[Effect-blurplus]\nOnlyQuickshell=true\nBlurStrength=1\nRefractionStrength=0\nBrightness=1\nContrast=1\n")
    fixture = "text-quality.qml" if options.text_quality else "shell.qml"
    shutil.copy2(Path(__file__).with_name(fixture), root / "shell/shell.qml")
    if options.fontconfig:
        (root / "config/fontconfig").mkdir()
        shutil.copy2(options.fontconfig, root / "config/fontconfig/fonts.conf")
    for name in ["desktop", "shared", "Kos"]:
        (root / "shell" / name).symlink_to(REPO / "shell" / name, target_is_directory=True)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(root / "runtime"), XDG_CONFIG_HOME=str(root / "config"),
        XDG_DATA_HOME=str(root / "data"), XDG_CACHE_HOME=str(root / "cache"), XDG_STATE_HOME=str(root / "state"),
        WAYLAND_DISPLAY="glass-contrast-test", DISPLAY="", QSG_RHI_BACKEND="opengl", LIBGL_ALWAYS_SOFTWARE="1",
        QT_PLUGIN_PATH=str(root / "plugins"), QML_IMPORT_PATH=str(options.qml),
        QML2_IMPORT_PATH=str(options.qml), KOS_SURFACE_SHAPE_MAX_VERSION=str(options.max_version),
        QT_SCALE_FACTOR=str(options.scale), KOS_GLASS_CONTRAST_OUTPUT=str(root / "captures"))
    env["QS_DISABLE_FILE_WATCHER"] = "1"
    env.pop("QT_QUICK_BACKEND", None)
    if options.text_quality:
        env["FC_LANG"] = "zh-cn"
    if options.natural_width:
        env["KOS_TEXT_QUALITY_NATURAL"] = "1"
    if options.legacy: env["KOS_SURFACE_SHAPE_MAX_VERSION"] = "8"
    if options.dynamic: env["KOS_GLASS_CONTRAST_DYNAMIC"] = "1"
    if options.scalar: env["KOS_SURFACE_SHAPE_MAX_VERSION"] = "9"
    if options.probe: env["KOS_GLASS_CONTRAST_PROBE"] = "1"
    if options.pixel:
        env["KOS_GLASS_CONTRAST_PIXEL"] = "1"
    with (root / "kwin.log").open("w+") as log:
        process = subprocess.Popen(["kwin_wayland", "--virtual", "--width", str(round(640 * options.scale)), "--height", str(round(240 * options.scale)),
            "--no-lockscreen", "--no-global-shortcuts", "--no-kactivities", "--socket", env["WAYLAND_DISPLAY"]],
            env=dict(env, QT_QPA_PLATFORM="offscreen", KWIN_COMPOSE="O2"), stdout=log, stderr=log)
        try:
            for _ in range(100):
                if (root / "runtime" / env["WAYLAND_DISPLAY"]).exists(): break
                if process.poll() is not None: raise RuntimeError("private KWin exited")
                time.sleep(.05)
            result = subprocess.run(["quickshell", "-p", str(root / "shell")],
                env=dict(env, QT_QPA_PLATFORM="wayland"), capture_output=True, text=True, timeout=18)
            output = result.stdout + result.stderr
            logs = REPO / "tmp/glass-contrast"
            logs.mkdir(parents=True, exist_ok=True)
            suffix = ("-text-quality" if options.text_quality else "-legacy" if options.legacy else "-dynamic" if options.dynamic
                else "-scalar" if options.scalar else "-pixel" if options.pixel else "-probe" if options.probe else "")
            suffix += f"-v{env['KOS_SURFACE_SHAPE_MAX_VERSION']}-scale{options.scale:g}"
            if options.fontconfig:
                suffix += "-fontconfig"
            if options.natural_width:
                suffix += "-natural"
            (logs / ("shell" + suffix + ".log")).write_text(output)
            log.flush(); log.seek(0)
            (logs / ("kwin" + suffix + ".log")).write_text(log.read())
            shutil.copytree(root / "captures", logs / ("captures" + suffix), dirs_exist_ok=True)
            print(output)
            assert result.returncode == 0 and "GLASS_CONTRAST_PASS" in output, output
            assert not re.search(r"ReferenceError|TypeError|Cannot assign|Unable to assign|is not a type"
                                 r"|does not have a property|No property matching|Failed to find.*source", output), output
            if options.pixel:
                check_split(root / "captures/split.png", root / "captures/reference.png")
                check_split(root / "captures/reverse.png", root / "captures/reference.png", reverse=True)
            if options.dynamic:
                check_sequence(root / "captures", "grey", allowed_flips=1)
                check_sequence(root / "captures", "flash", allowed_flips=2)
            if options.text_quality:
                report = report_text_quality(root / "captures")
                shutil.copy2(report, logs / ("captures" + suffix) / report.name)
                print("TEXT_QUALITY_REPORT", logs / ("captures" + suffix) / "text-quality.json")
                if options.check_text:
                    check_aligned_quality(report)
                    print("TEXT_QUALITY_ALIGNED_PASS")
        finally:
            process.terminate()
            try: process.wait(timeout=4)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
