//@ pragma UseQApplication
import QtQuick
import QtQuick.Window
import Quickshell
import Quickshell.Wayland
import qs.desktop.modules.common

// Diagnostic only: keep the string, font, geometry and background fixed while
// changing the foreground pipeline. A synthetic map separates client sampling
// from compositor feedback, temporal filtering and the wallpaper's contrast.
ShellRoot {
    id: root
    property int mode: 0
    property int backdrop: 0
    property bool capturing: false
    readonly property bool naturalWidth: Quickshell.env("KOS_TEXT_QUALITY_NATURAL") === "1"
    readonly property var modes: ["native", "glass-direct", "layer", "adaptive", "snap-native", "snap-adaptive"]
    readonly property var backdrops: ["light", "dark", "split"]
    readonly property color ink: backdrop === 1 ? "white" : Qt.rgba(0.06, 0.06, 0.07, 1)
    readonly property var cases: {
        const rows = []
        for (const size of [10, 11, 12, 13, 16]) {
            for (const offset of [0, 0.5]) rows.push({size: size, offset: offset})
        }
        return rows
    }
    function pixelOffset(item) {
        const point = item.parent.mapToItem(null, item.x, item.y)
        const dpr = frame.Screen.devicePixelRatio
        return Qt.point(Math.round(point.x * dpr) / dpr - point.x,
            Math.round(point.y * dpr) / dpr - point.y)
    }

    QtObject {
        id: inkPanel
        readonly property bool adaptiveInkReady: true
        readonly property bool backdropInkMapReady: true
        // Force the mixed-map path even on a uniform background: this is the
        // controlled comparison that bypasses production's uniform-map shortcut.
        readonly property bool backdropInkUniform: false
        readonly property bool backdropInkDark: root.backdrop !== 1
        readonly property color foregroundColor: root.ink
        readonly property Item backdropMapItem: frame
        readonly property Item backdropInkMapTexture: maskTexture
    }

    PanelWindow {
        id: window
        anchors { top: true; bottom: true; left: true; right: true }
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.layer: WlrLayer.Top
        color: "transparent"
        Item {
            id: frame
            width: 640
            height: 240
            readonly property var glassContrastSource: inkPanel
            Rectangle { anchors.fill: parent; color: root.backdrop === 1 ? "black" : "white" }
            Rectangle {
                x: parent.width / 2; width: parent.width / 2; height: parent.height
                visible: root.backdrop === 2
                color: "black"
            }
            Repeater {
                id: sampleRows
                model: root.cases
                delegate: Item {
                    id: row
                    required property var modelData
                    required property int index
                    x: 24 + modelData.offset
                    y: 8 + index * 22 + modelData.offset
                    width: frame.width - 48
                    height: 22
                    readonly property string sample: "办公文档  WPS Office  开发  Konsole 终端  Wi-Fi Demo"
                    readonly property alias sampleLabel: label
                    ShellText {
                        id: nativeLabel
                        anchors.centerIn: parent
                        width: root.naturalWidth ? Math.min(parent.width, implicitWidth) : parent.width
                        height: parent.height
                        visible: root.mode === 0 || root.mode === 4
                        transform: Translate {
                            x: root.mode === 4 ? root.pixelOffset(nativeLabel).x : 0
                            y: root.mode === 4 ? root.pixelOffset(nativeLabel).y : 0
                        }
                        text: row.sample
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: root.ink
                        font.pixelSize: row.modelData.size
                        font.weight: Font.Medium
                        font.hintingPreference: Font.PreferVerticalHinting
                    }
                    GlassText {
                        id: label
                        anchors.centerIn: parent
                        width: root.naturalWidth ? Math.min(parent.width, implicitWidth) : parent.width
                        height: parent.height
                        visible: root.mode !== 0 && root.mode !== 4
                        pixelAlignedInk: root.mode === 5
                        text: row.sample
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: root.ink
                        font.pixelSize: row.modelData.size
                        font.weight: Font.Medium
                        shadowEnabled: false
                        layer.enabled: root.mode >= 2
                        layer.effect: AdaptiveInkEffect {
                            target: label
                            panel: inkPanel
                            inkColor: label.color
                            inkAlpha: 1
                            adaptive: root.mode === 3 || root.mode === 5
                        }
                    }
                }
            }
        }
        Item {
            id: maskPattern
            visible: false
            width: 2; height: 1
            Rectangle { width: 1; height: 1; color: root.backdrop === 1 ? "black" : "white" }
            Rectangle { x: 1; width: 1; height: 1; color: root.backdrop === 0 ? "white" : "black" }
        }
        ShaderEffectSource {
            id: maskTexture
            sourceItem: maskPattern
            hideSource: true
            visible: false
            width: 2; height: 1
            live: true
            textureSize: Qt.size(2, 1)
            smooth: true
        }
    }

    Timer {
        interval: 500
        repeat: true
        running: !root.capturing
        onTriggered: {
            root.capturing = true
            const name = root.backdrops[root.backdrop] + "-" + root.modes[root.mode]
            const dpr = frame.Screen.devicePixelRatio
            if (root.mode === 0 && root.backdrop === 0) {
                console.log("TEXT_QUALITY_GEOMETRY", frame.width, frame.height, dpr)
                for (let i = 0; i < sampleRows.count; ++i) {
                    const label = sampleRows.itemAt(i).sampleLabel
                    console.log("TEXT_QUALITY_ROW", i, label.width, label.height)
                }
            }
            const accepted = frame.grabToImage(function(result) {
                if (!result.saveToFile(Quickshell.env("KOS_GLASS_CONTRAST_OUTPUT") + "/" + name + ".png")) {
                    console.error("GLASS_CONTRAST_FAIL text-quality capture", name)
                    Qt.exit(1)
                    return
                }
                console.log("TEXT_QUALITY_CAPTURE", name, dpr)
                root.mode++
                if (root.mode === root.modes.length) {
                    root.mode = 0
                    root.backdrop++
                }
                if (root.backdrop === root.backdrops.length) {
                    console.log("GLASS_CONTRAST_PASS text-quality captures only; inspect pixel report")
                    Qt.quit()
                } else {
                    root.capturing = false
                }
            })
            if (!accepted) {
                console.error("GLASS_CONTRAST_FAIL text-quality grab rejected")
                Qt.exit(1)
            }
        }
    }
}
