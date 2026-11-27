import QtQuick
import Backhaul

// Two people side by side in one picture. When one side has no picture and the
// operator chose so, the other side takes the whole area.
Rectangle {
    id: root
    property bool hideCursor: true
    property string placeholder: ""
    readonly property int split: app.twoShotSplit   // 0 both, 1 left only, 2 right only
    // Two 16:9 boxes next to each other (black above and below) instead of two tall halves.
    readonly property bool boxed: split === 0 && !app.twoShotFill
    readonly property int half: Math.floor(width / 2)
    readonly property int boxHeight: boxed ? Math.round(half * 9 / 16) : height
    color: "black"
    clip: true

    OutputView {
        visible: root.split !== 2
        x: 0
        y: Math.round((root.height - height) / 2)
        width: root.split === 0 ? root.half : root.width
        height: root.boxHeight
        slotRow: app.twoShotLeft
        fill: root.split === 0 && app.twoShotFill
        hideCursor: root.hideCursor
        placeholder: root.placeholder
    }
    OutputView {
        visible: root.split !== 1
        x: root.split === 0 ? root.half : 0
        y: Math.round((root.height - height) / 2)
        width: root.width - x
        height: root.boxHeight
        slotRow: app.twoShotRight
        fill: root.split === 0 && app.twoShotFill
        hideCursor: root.hideCursor
        placeholder: root.placeholder
    }
}
