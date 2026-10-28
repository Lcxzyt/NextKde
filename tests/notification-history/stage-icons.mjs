import { copyFileSync, mkdirSync, symlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
export function stageIcons(directory) {
    const common = join(directory, "desktop/modules/common");
    mkdirSync(common, {recursive: true});
    symlinkSync(new URL("../../shell/shared", import.meta.url), join(directory, "shared"), "dir");
    const declarations = ["singleton AppearanceTokens 1.0 AppearanceTokens.qml",
        "singleton IconAppearanceService 1.0 IconAppearanceService.qml"];
    // These fixtures have no compositor-backed adaptive panel. Keep their
    // theme fallback explicit while staging production text/icon controls.
    writeFileSync(join(common, "IconAppearanceService.qml"), `pragma Singleton
import QtQuick
QtObject {
    function glassContrastPanel(target) { return null }
    function perPixelInk(panel, color) { return false }
}
`);
    writeFileSync(join(common, "AppearanceTokens.qml"), `pragma Singleton
import QtQuick
QtObject { readonly property bool isDarkTheme: true }
`);
    // The unit fixture has no Wayland shape plugin. Compositor acceptance
    // exercises the real foreground-only probe separately.
    writeFileSync(join(common, "BackdropInkProbe.qml"), `import QtQuick
Item {
    property bool active: true
    readonly property bool adaptiveInkReady: false
    readonly property color foregroundColor: "white"
}
`);
    declarations.push("BackdropInkProbe 1.0 BackdropInkProbe.qml");
    for (const name of ["AppIcon", "BundledIcon", "ShellText", "GlassText", "AdaptiveInkEffect", "BundledIcons", "AppPresentationService", "SystemIconResolver", "IconThemeReloadService"]) {
        copyFileSync(new URL(`../../shell/desktop/modules/common/${name}.qml`, import.meta.url), join(common, `${name}.qml`));
        declarations.push(`${["AppIcon", "BundledIcon", "ShellText", "GlassText", "AdaptiveInkEffect"].includes(name) ? "" : "singleton "}${name} 1.0 ${name}.qml`);
    }
    writeFileSync(join(common, "qmldir"), `module qs.desktop.modules.common\n${declarations.join("\n")}\n`);
    copyFileSync(new URL("../../shell/desktop/modules/bar/NotificationSourceIcon.qml", import.meta.url), join(directory, "NotificationSourceIcon.qml"));
}
