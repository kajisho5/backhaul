pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#0a0b0c"
    readonly property color panel: "#141618"
    readonly property color line: "#2a2d31"
    readonly property color ink: "#f2f2f0"
    readonly property color mute: "#9a9fa6"
    readonly property color picture: "#050506"
    readonly property color control: "#1e2125"

    readonly property color ok: "#3ddc84"
    readonly property color camoff: "#8fb0e0"
    readonly property color muted: "#ffc83d"
    readonly property color unstable: "#ff8552"
    readonly property color lost: "#ff4d5e"

    function stateColor(state) {
        switch (state) {
        case "ok": return ok
        case "camoff": return camoff
        case "mute": return muted
        case "unstable": return unstable
        case "lost": return lost
        default: return line
        }
    }
}
