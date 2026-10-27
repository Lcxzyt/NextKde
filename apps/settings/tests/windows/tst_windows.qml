import QtQuick
import QtTest
import "../../../../shared/qml/i18n"
Item {
    property var windowSettings: backend
    QtObject {
        id: backend
        property var state: ({enabled:true, radius:20, shadow:true, hideAnimation:"scale", closeAnimation:"scale", buttonGlyphMode:"always", minimizeDuration:300, restoreDuration:200})
        property string error: ""
        property var calls: []
        function refresh() {}
        function change(key, value) {
            calls = calls.concat([{key:key, value:value}])
            state = Object.assign({}, state, {[key]:value})
        }
        function setRadius(value) { change("radius", value); return true }
        function setShadow(value) { change("shadow", value); return true }
        function setTakeover(value) { change("enabled", value); return true }
        function setAnimation(kind, value) { change(kind + "Animation", value); return true }
        function setButtonGlyphMode(value) { change("buttonGlyphMode", value); return true }
        function setAnimationDuration(kind, value) { change(kind + "Duration", value); return true }
    }
    TestCase {
        name: "WindowSettingsInteractions"
        when: windowShown
        property var app: null
        function init() {
            backend.calls = []
            backend.state = {enabled:true, radius:20, shadow:true, hideAnimation:"scale", closeAnimation:"scale", buttonGlyphMode:"always", minimizeDuration:300, restoreDuration:200}
            const component = Qt.createComponent("../../main.qml")
            compare(component.status, Component.Ready, component.errorString())
            app = component.createObject(null, {currentPage:11})
            verify(app !== null)
            tryVerify(() => findChild(app.contentItem, "windowAppearancePage") !== null)
        }
        function cleanup() { app.destroy(); app = null }
        function control(name) {
            const item = findChild(app.contentItem, name)
            verify(item !== null, name)
            verify(waitForRendering(item))
            return item
        }
        function test_switches() {
            const shadow = control("windowShadowSwitch")
            mouseClick(shadow, shadow.width/2, shadow.height/2)
            compare(backend.state.shadow, false)
            const takeover = control("windowTakeoverSwitch")
            mouseClick(takeover, takeover.width/2, takeover.height/2)
            compare(backend.state.enabled, false)
            verify(!control("windowRadiusSlider").enabled)
            compare(backend.state.hideAnimation, "scale", "appearance switch leaves animations independent")
        }
        function test_radius_stops() {
            const slider = control("windowRadiusSlider")
            const stops = [0,8,12,20,28,36]
            for (let i=0; i<stops.length; ++i) {
                mouseClick(slider, slider.edgeInset + slider.travel * i/5, slider.height/2)
                compare(backend.state.radius, stops[i])
            }
            const count=backend.calls.length
            slider.previewChanged(0.4)
            compare(backend.calls.length, count, "preview does not save")
            slider.canceled()
            compare(backend.state.radius, 36, "cancel preserves saved radius")
        }
        function test_animation_choices() {
            for (const kind of ["hide", "close"]) {
                const picker=control(kind === "hide" ? "windowHideAnimation" : "windowCloseAnimation")
                const styles=kind === "hide" ? ["none","scale","genie"] : ["none","fade","scale"]
                for (let i=0;i<3;++i) {
                    mouseClick(picker, picker.width * (i+0.5)/3, picker.height/2)
                    compare(backend.state[kind+"Animation"], styles[i])
                }
            }
        }
        function test_button_glyph_mode() {
            const picker = control("windowButtonGlyphMode")
            const modes = ["always", "hover"]
            for (let i = 0; i < 2; ++i) {
                mouseClick(picker, picker.width * (i + 0.5) / 2, picker.height / 2)
                compare(backend.state.buttonGlyphMode, modes[i])
            }
        }
        function test_duration_sliders() {
            for (const kind of ["minimize", "restore"]) {
                const slider = control(kind === "minimize" ? "windowMinimizeDurationSlider" : "windowRestoreDurationSlider")
                const minimum = kind === "minimize" ? 160 : 80
                const label = control(kind + "DurationValue")
                compare(label.text, (kind === "minimize" ? 300 : 200) + " ms")
                for (const position of [0, 0.5, 1]) {
                    mouseClick(slider, slider.edgeInset + slider.travel * position, slider.height/2)
                    const expected = Math.round((minimum + position * (1200 - minimum)) / 20) * 20
                    compare(backend.state[kind + "Duration"], expected)
                    compare(label.text, expected + " ms")
                }
                const count = backend.calls.length
                slider.previewChanged(0.5)
                compare(backend.calls.length, count, "preview does not save")
                verify(label.text !== "1200 ms", "preview updates the displayed duration")
                slider.canceled()
                compare(label.text, "1200 ms", "cancel restores the saved value")
                compare(backend.state[kind + "Duration"], 1200)
            }
            compare(backend.state.closeAnimation, "scale", "duration controls leave closing independent")
        }
        function test_duration_disabled_without_animation() {
            const minimize = control("windowMinimizeDurationSlider")
            const restore = control("windowRestoreDurationSlider")
            backend.setTakeover(false)
            verify(minimize.enabled && restore.enabled, "animation timing is independent of window appearance")
            backend.setAnimation("hide", "none")
            verify(!minimize.enabled && !restore.enabled)
            const count = backend.calls.length
            mouseClick(minimize, minimize.width/2, minimize.height/2)
            mouseClick(restore, restore.width/2, restore.height/2)
            compare(backend.calls.length, count)
            backend.setAnimation("hide", "genie")
            verify(minimize.enabled && restore.enabled)
            compare(backend.state.minimizeDuration, 300)
            compare(backend.state.restoreDuration, 200)
        }
        function test_duration_layout() {
            const previousLanguage = I18n.language
            try {
                app.width = app.minimumWidth
                for (const language of ["en", "zh-CN"]) {
                    I18n.language = language
                    for (const material of [false, true]) {
                        app.shellStyle = material ? "material" : "macos"
                        wait(100)
                        for (const kind of ["minimize", "restore"]) {
                            const label = findChild(app.contentItem, kind + "DurationValue")
                            const slider = findChild(app.contentItem, kind === "minimize" ? "windowMinimizeDurationSlider" : "windowRestoreDurationSlider")
                            verify(label !== null && slider !== null)
                            verify(label.x + label.width <= slider.x, "duration and slider do not overlap")
                            verify(slider.x + slider.width <= slider.parent.width, "slider stays inside the row")
                        }
                    }
                }
                app.shellStyle = "macos"
                wait(100)
            } finally {
                I18n.language = previousLanguage
            }
        }
    }
}
