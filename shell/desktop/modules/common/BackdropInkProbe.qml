import QtQuick
import Kos.SurfaceShape 1.0

// A foreground-only sampling region. Protocol v11 does not draw any material
// for this shape, so floating labels read their own background without adding
// a glass plate. Older native modules/compositors publish no probe at all.
Item {
    id: root
    property bool active: true
    readonly property var glassContrastSource: root
    readonly property Item backdropMapItem: root
    readonly property real backdropLuminance: ("backdropLuminance" in shape)
        ? shape.backdropLuminance : -1
    readonly property bool adaptiveInkReady: active && backdropLuminance >= 0
    readonly property bool backdropMapReady: adaptiveInkReady && ("backdropMapSource" in shape)
        && shape.backdropMapSource.length > 0
    readonly property bool backdropInkUniform: ("backdropInkUniform" in shape) && shape.backdropInkUniform
    readonly property bool backdropInkDark: ("backdropInkDark" in shape) && shape.backdropInkDark
    readonly property bool backdropInkMapReady: adaptiveInkReady
        && inkImage.source.toString().length > 0 && inkImage.status === Image.Ready
    readonly property alias backdropInkMapTexture: inkImage
    readonly property color foregroundColor: contrastInk(1)
    function contrastInk(alpha) {
        const dark = backdropInkMapReady && backdropInkUniform ? backdropInkDark : backdropLuminance >= 0.46
        return dark ? Qt.rgba(0.06, 0.06, 0.07, alpha) : Qt.rgba(1, 1, 1, alpha)
    }
    function backdropAt(x, y) {
        return backdropMapReady && ("backdropAt" in shape) ? shape.backdropAt(x, y) : -1
    }
    Image {
        id: inkImage
        visible: false
        width: 1; height: 1
        source: root.adaptiveInkReady && ("backdropInkMapSource" in shape) ? shape.backdropInkMapSource : ""
        cache: false
        asynchronous: false
        retainWhileLoading: true
        smooth: true
    }
    property var surfaceShape: SurfaceShape {
        id: shape
        target: ("contrastOnly" in shape) ? root : null
        enabled: root.active && root.visible
        blurSigma: 0
        effectType: SurfaceShape.Frosted
        material: SurfaceShape.Clear
        radius: 0
    }
    property list<QtObject> feedbackBindings: [
        Binding {
            target: ("contrastFeedback" in shape) ? shape : null
            property: "contrastFeedback"
            value: true
        },
        Binding {
            target: ("contrastOnly" in shape) ? shape : null
            property: "contrastOnly"
            value: true
        }
    ]
}
