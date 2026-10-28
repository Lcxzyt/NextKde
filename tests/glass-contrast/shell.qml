import QtQuick
import Quickshell
import Quickshell.Wayland
import qs.desktop.modules.common

ShellRoot {
    id: root
    property bool legacy: Quickshell.env("KOS_SURFACE_SHAPE_MAX_VERSION") === "8"
    property bool scalarOnly: Quickshell.env("KOS_SURFACE_SHAPE_MAX_VERSION") === "9"
    property bool perPixel: Quickshell.env("KOS_GLASS_CONTRAST_PIXEL") === "1"
    property bool dynamic: Quickshell.env("KOS_GLASS_CONTRAST_DYNAMIC") === "1"
    property bool probeOnly: Quickshell.env("KOS_GLASS_CONTRAST_PROBE") === "1"
    property int capturesPending: 0
    property int captureSerial: 0
    property bool capturingSplit: false
    property string observedInkSource: ""
    property string initialInkSource: ""
    property int inkChangedTick: 0
    function capture(item, name, done) {
        capturesPending++
        item.grabToImage(function(result) {
            if (!result.saveToFile(Quickshell.env("KOS_GLASS_CONTRAST_OUTPUT") + "/" + name + ".png")) {
                console.error("GLASS_CONTRAST_FAIL save", name); Qt.exit(1); return
            }
            capturesPending--
            if (done) done()
        })
    }
    property real brightness: 0
    property int flips: 0
    property int feedback: 0
    property int transitionFrames: 0
    property int phaseStarted: 0
    property int phaseFlips: 0
    property bool rampFinished: false
    property int stage: 0
    property bool reversed: false
    property int ticks: 0
    PanelWindow {
        id: background
        anchors { top: true; bottom: true; left: true; right: true }
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.layer: WlrLayer.Background
        color: "black"
        Rectangle { width: parent.width / 2; height: parent.height; color: root.stage >= 2 ? Qt.rgba(root.brightness, root.brightness, root.brightness, 1)
            : root.reversed ? "black" : "white" }
        Rectangle { x: parent.width / 2; width: parent.width / 2; height: parent.height; color: root.stage >= 2 ? Qt.rgba(1-root.brightness, 1-root.brightness, 1-root.brightness, 1)
            : root.reversed ? "white" : "black" }
        // Probe above the card sees the opposite brightness, like a window
        // edge separating a floating notification header from its first card.
        Rectangle { width: parent.width; height: 28; visible: root.probeOnly
            color: root.reversed ? "black" : "white" }
    }
    PanelWindow {
        id: foreground
        anchors { top: true; bottom: true; left: true; right: true }
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.layer: WlrLayer.Top
        color: "transparent"
        LiquidGlassPanel {
            id: lightPanel
            onDarkForegroundChanged: if (root.stage >= 2) root.flips++
            onBackdropLuminanceChanged: if (root.stage === 2) root.feedback++
            onInkMixChanged: if (root.stage === 2 && inkMix > 0.1 && inkMix < 0.9) root.transitionFrames++
            x: 20; y: 30; width: 200; height: 120; radius: 16
            adaptiveInkEnabled: true
            blurSigma: 0
            scrimEnabled: false
            Item {
                id: sampleCapture
                anchors.centerIn: parent
                width: parent.width; height: 42
                GlassText { id: sampleLabel; anchors.centerIn: parent; text: "Aa 中文 ☀"; color: lightPanel.foregroundColor; font.pixelSize: 20 }
            }
            BundledIcon { id: symbol; x: 8; y: 8; name: "settings"; color: AppearanceTokens.content.glassInk(undefined, parent) }
            Rectangle { x: 150; y: 80; width: 30; height: 30; color: "#0088ff"
                Text { id: activeGlyph; text: "✓"; color: "white" }
            }
        }
        // Straddles the background's centre split: left half over the left
        // half-screen, right half over the right one.
        LiquidGlassPanel {
            id: splitPanel
            x: 220; y: 160; width: 200; height: 60; radius: 16
            adaptiveInkEnabled: true
            blurSigma: 0
            scrimEnabled: false
            Item {
                id: splitCapture
                anchors.fill: parent
                GlassText { id: splitLabel; objectName: "splitLabel"; anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    text: "明暗分割 Split"; color: splitPanel.foregroundColor; font.pixelSize: 22 }
            }
            BundledIcon { id: splitIcon; x: 90; y: 0; size: 14; name: "settings"
                color: AppearanceTokens.content.glassInk(undefined, parent) }
        }
        LiquidGlassPanel {
            id: darkPanel
            x: 340; y: 30; width: 200; height: 120; radius: 16
            adaptiveInkEnabled: true
            blurSigma: 0
            scrimEnabled: false
            GlassText { anchors.centerIn: parent; text: "Aa 中文 ☀"; color: darkPanel.foregroundColor; font.pixelSize: 20 }
        }
        Item {
            id: referenceCapture
            width: 200; height: 60
            ShellText { anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                text: "明暗分割 Split"; color: "white"; font.pixelSize: 22
                font.hintingPreference: Font.PreferVerticalHinting }
        }
        BackdropInkProbe {
            id: headerProbe
            x: 340; y: 0; width: 200; height: 24
            active: root.probeOnly
            GlassText { anchors.centerIn: parent; text: "Notifications"; color: headerProbe.foregroundColor; font.pixelSize: 16 }
        }
    }
    NumberAnimation { id: rampUp; target: root; property: "brightness"; from: 0; to: 1; duration: 900; onFinished: rampDown.start() }
    NumberAnimation { id: rampDown; target: root; property: "brightness"; from: 1; to: 0; duration: 900; onFinished: root.rampFinished = true }
    Timer {
        id: rapid; interval: 40; repeat: true
        onTriggered: root.brightness = root.stage === 3
            ? (root.brightness > 0.5 ? 0.49 : 0.51) : (root.brightness > 0.5 ? 0 : 1)
    }
    Timer {
        interval: 20; running: true; repeat: true
        onTriggered: {
            root.ticks++
            if (root.ticks > 500) {
                console.error("GLASS_CONTRAST_FAIL timeout", lightPanel.backdropLuminance, darkPanel.backdropLuminance)
                Qt.exit(1); return
            }
            if (root.probeOnly) {
                if (Quickshell.env("KOS_SURFACE_SHAPE_MAX_VERSION") === "10") {
                    if (root.ticks < 50) return
                    if (headerProbe.surfaceShape.active || headerProbe.adaptiveInkReady) {
                        console.error("GLASS_CONTRAST_FAIL v10 drew a probe"); Qt.exit(1); return
                    }
                    console.error("GLASS_CONTRAST_PASS v10 publishes no unsupported probe"); Qt.quit(); return
                }
                if (!headerProbe.backdropInkMapReady || !darkPanel.backdropInkMapReady
                        || !headerProbe.backdropInkUniform || !darkPanel.backdropInkUniform) return
                if (headerProbe.backdropInkDark === !root.reversed
                        && darkPanel.backdropInkDark === root.reversed) {
                    if (!root.reversed) root.reversed = true
                    else {
                        console.error("GLASS_CONTRAST_PASS header samples independently above window edge")
                        Qt.quit()
                    }
                }
                return
            }
            if (root.legacy && root.ticks > 40) {
                if (lightPanel.adaptiveInkReady || darkPanel.adaptiveInkReady
                        || lightPanel.backdropLuminance >= 0 || darkPanel.backdropLuminance >= 0
                        || symbol.color.toString() !== AppearanceTokens.content.glassInk().toString()) {
                    console.error("GLASS_CONTRAST_FAIL legacy fallback"); Qt.exit(1); return
                }
                console.error("GLASS_CONTRAST_PASS legacy v8 theme fallback")
                Qt.quit(); return
            }
            if (root.scalarOnly && lightPanel.adaptiveInkReady && root.ticks > 40) {
                if (lightPanel.backdropMapReady || splitLabel.perPixelInk) {
                    console.error("GLASS_CONTRAST_FAIL v9 client received a map"); Qt.exit(1); return
                }
                console.error("GLASS_CONTRAST_PASS v9 scalar fallback without per-pixel map")
                Qt.quit(); return
            }
            if (root.perPixel) {
                if (root.capturingSplit || !splitPanel.backdropInkMapReady) return
                const inkSource = splitPanel.backdropInkMapTexture.source.toString()
                if (inkSource !== root.observedInkSource) {
                    root.observedInkSource = inkSource
                    root.inkChangedTick = root.ticks
                }
                if (root.ticks - root.inkChangedTick < 10
                        || (root.reversed && inkSource === root.initialInkSource)) return
                // Map orientation: the left edge of the shape reads the left
                // (white) half, the right edge the right (black) half.
                const left = splitPanel.backdropAt(0.1, 0.5), right = splitPanel.backdropAt(0.9, 0.5)
                const top = lightPanel.backdropAt(0.5, 0.1), bottom = lightPanel.backdropAt(0.5, 0.9)
                const correct = root.reversed
                    ? left < 0.15 && right > 0.85 && top < 0.15 && bottom < 0.15
                    : left > 0.85 && right < 0.15 && top > 0.85 && bottom > 0.85
                if (!correct) {
                    if (root.ticks > 300) {
                        console.error("GLASS_CONTRAST_FAIL map orientation", left, right, top, bottom); Qt.exit(1)
                    }
                    return
                }
                if (!splitLabel.perPixelInk || !splitIcon.perPixelInk) {
                    console.error("GLASS_CONTRAST_FAIL glyphs not per-pixel", splitLabel.perPixelInk, splitIcon.perPixelInk); Qt.exit(1); return
                }
                root.capturingSplit = true
                root.capture(splitCapture, root.reversed ? "reverse" : "split", function() {
                    if (root.reversed) {
                        console.error("GLASS_CONTRAST_PIXEL_MAP", left.toFixed(2), right.toFixed(2))
                        console.error("GLASS_CONTRAST_PASS captured final split/reversed glyphs with unchanged panel mean")
                        Qt.quit()
                        return
                    }
                    root.capture(referenceCapture, "reference", function() {
                        root.initialInkSource = inkSource
                        root.reversed = true
                        root.capturingSplit = false
                    })
                })
                return
            }
            if (!lightPanel.adaptiveInkReady || !darkPanel.adaptiveInkReady) return
            if (root.scalarOnly) return
            if (root.dynamic && (root.stage === 3 || root.stage === 4)
                    && root.ticks - root.phaseStarted > 20 && root.ticks % 5 === 0) {
                root.capture(sampleCapture, (root.stage === 3 ? "grey-" : "flash-") + String(root.captureSerial++).padStart(3, "0"))
            }
            if (root.stage === 0 && lightPanel.darkForeground && !darkPanel.darkForeground
                    && !lightPanel.inkTransitionActive && !darkPanel.inkTransitionActive) {
                if (symbol.color.r > 0.2 || activeGlyph.color.r < 0.9) {
                    console.error("GLASS_CONTRAST_FAIL initial symbol or active-state color"); Qt.exit(1); return
                }
                console.error("GLASS_CONTRAST_INITIAL_PASS", lightPanel.backdropLuminance, darkPanel.backdropLuminance)
                root.stage = 1
                root.reversed = true
            } else if (root.stage === 1 && !lightPanel.darkForeground && darkPanel.darkForeground
                    && !lightPanel.inkTransitionActive && !darkPanel.inkTransitionActive) {
                if (lightPanel.foregroundColor.r < 0.9 || darkPanel.foregroundColor.r > 0.2) {
                    console.error("GLASS_CONTRAST_FAIL foreground colors"); Qt.exit(1); return
                }
                if (symbol.color.r < 0.9 || activeGlyph.color.r < 0.9) {
                    console.error("GLASS_CONTRAST_FAIL symbol or active-state color"); Qt.exit(1); return
                }
                if (!root.dynamic) {
                    console.error("GLASS_CONTRAST_PASS actual-window background swap and independent panel foreground")
                    Qt.quit(); return
                }
                if (!sampleLabel.contrastGuard || sampleLabel.style !== Text.Normal) {
                    console.error("GLASS_CONTRAST_FAIL missing clean adaptive text"); Qt.exit(1); return
                }
                root.stage = 2
                root.flips = 0
                rampUp.start()
            } else if (root.stage === 2 && root.rampFinished
                    && !lightPanel.darkForeground && !lightPanel.inkTransitionActive) {
                if (root.flips !== 2 || root.feedback < 16 || root.transitionFrames < 4) {
                    console.error("GLASS_CONTRAST_FAIL ramp", root.flips, root.feedback, root.transitionFrames); Qt.exit(1); return
                }
                console.error("GLASS_CONTRAST_RAMP_PASS", root.flips, root.feedback, root.transitionFrames)
                root.stage = 3
                root.phaseStarted = root.ticks
                root.phaseFlips = root.flips
                root.brightness = 0.5
                rapid.start()
            } else if (root.stage === 3 && root.ticks - root.phaseStarted > 50 && root.capturesPending === 0) {
                if (root.flips !== root.phaseFlips) {
                    console.error("GLASS_CONTRAST_FAIL grey noise flicker"); Qt.exit(1); return
                }
                root.stage = 4
                root.phaseStarted = root.ticks
            } else if (root.stage === 4 && root.ticks - root.phaseStarted > 60 && root.capturesPending === 0) {
                if (root.flips - root.phaseFlips > 2) {
                    console.error("GLASS_CONTRAST_FAIL fast flash flicker", root.flips-root.phaseFlips); Qt.exit(1); return
                }
                console.error("GLASS_CONTRAST_FLASH_PASS", root.flips-root.phaseFlips)
                rapid.stop()
                root.stage = 5
                root.brightness = 1
            } else if (root.stage === 5 && lightPanel.darkForeground && !lightPanel.inkTransitionActive
                    && lightPanel.backdropInkMapReady && lightPanel.backdropInkUniform && lightPanel.backdropInkDark) {
                if (sampleLabel.perPixelInk || sampleLabel.layer.enabled) {
                    console.error("GLASS_CONTRAST_FAIL uniform text still rendered offscreen"); Qt.exit(1); return
                }
                console.error("GLASS_CONTRAST_PASS dynamic ramp, grey noise, rapid flashes and settled endpoint")
                Qt.quit()
            }
        }
    }
}
