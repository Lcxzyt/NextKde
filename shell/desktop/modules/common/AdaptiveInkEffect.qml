import QtQuick

// Layer effect that re-inks a neutral glyph pixel by pixel from the glass
// panel's compositor backdrop map. Use as `layer.effect` on the glyph item;
// the layer supplies `source`, whose alpha is the glyph's coverage.
//
// Each pixel reads the backdrop brightness directly behind it, so one label
// can be dark over a bright window and light over a dark one at the same
// time, split along the backdrop's own edge.
ShaderEffect {
    id: root
    // layer.samplerName defaults to "source". Declare the property so Qt can
    // actually pass the glyph texture to the shader, rather than an unset sampler.
    property variant source
    property Item target: null
    property var panel: IconAppearanceService.glassContrastPanel(target)
    // The caller's colour: the ink before the first map arrives, and on items
    // that opt out (an active toggle's glyph on its own accent disc).
    property color inkColor: "white"
    property bool adaptive: true
    // A mask's alpha is pure coverage, so the caller's alpha is applied here.
    // Text and Canvas already carry their colour's alpha; they set 1.
    property real inkAlpha: inkColor.a
    property color lightInk: "white"
    property color darkInk: Qt.rgba(0.06, 0.06, 0.07, 1)
    // Compatibility with the previous .qsb until shaders are rebuilt: its
    // smoothstep now spans only a mask boundary, not a range of grey backdrop.
    property real band: 0.0001

    // The glyph's rectangle inside the published shape, normalized to the map.
    // Only motion *between* glyph and shape matters. Track it on frames rather
    // than binding to every ancestor: a layer effect is destroyed synchronously
    // when its host's layer toggles, and must
    // not own handlers on objects whose signals can trigger that toggle.
    property vector4d mapRect: Qt.vector4d(0, 0, 1, 1)
    function refresh() {
        // Share this frame callback with GlassText's physical-pixel alignment.
        // Map after the correction, so backdrop ink still follows the glyph.
        if (target && "refreshInkPixelAlignment" in target)
            target.refreshInkPixelAlignment()
        const item = panel ? panel.backdropMapItem : null
        if (!target || !item || item.width <= 0 || item.height <= 0) return
        const r = target.mapToItem(item, 0, 0, target.width, target.height)
        const next = Qt.vector4d(r.x / item.width, r.y / item.height,
            r.width / item.width, r.height / item.height)
        if (next.x !== mapRect.x || next.y !== mapRect.y
                || next.z !== mapRect.z || next.w !== mapRect.w) mapRect = next
    }
    FrameAnimation {
        running: root.visible && root.mapEnabled > 0
        onTriggered: root.refresh()
    }
    Component.onCompleted: refresh()

    property variant backdropMap: panel ? panel.backdropInkMapTexture : null
    readonly property real mapEnabled: adaptive && panel && panel.backdropInkMapReady ? 1 : 0
    readonly property color fallbackInk: inkColor
    fragmentShader: Qt.resolvedUrl("../../shaders/adaptive_ink.frag.qsb")
}
