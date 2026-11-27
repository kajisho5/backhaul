import QtQuick
import Backhaul

// Two people side by side in one picture. When one side has no picture and the
// operator chose so, the other side takes the whole area.
Rectangle {
    id: root
    property bool hideCursor: true
    property string placeholder: ""
    readonly property int split: app.twoShotSplit   // 0 both, 1 left only, 2 right only
    color: "black"
    clip: true

    OutputView {
        visible: root.split !== 2
        x: 0
        width: root.split === 0 ? Math.floor(root.width / 2) : root.width
        height: root.height
        slotRow: app.twoShotLeft
        fill: root.split === 0 && app.twoShotFill
        hideCursor: root.hideCursor
        placeholder: root.placeholder
    }
    OutputView {
        visible: root.split !== 1
        x: root.split === 0 ? Math.floor(root.width / 2) : 0
        width: root.width - x
        height: root.height
        slotRow: app.twoShotRight
        fill: root.split === 0 && app.twoShotFill
        hideCursor: root.hideCursor
        placeholder: root.placeholder
    }
}
