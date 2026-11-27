import QtQuick
import Backhaul

// A horizontal level meter with a short label in front. `level` is 0..1.
Item {
    id: root
    property string label: ""
    property real level: 0
    implicitHeight: 14
    Accessible.role: Accessible.Indicator
    Accessible.name: "レベル " + label

    Text {
        id: caption
        anchors.verticalCenter: parent.verticalCenter
        width: 22
        text: root.label
        color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 11
    }
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: caption.right
        anchors.leftMargin: 6
        anchors.right: parent.right
        height: 8
        color: Theme.picture
        border.width: 1
        border.color: Theme.line
        Rectangle {
            x: 1; y: 1
            height: parent.height - 2
            width: (parent.width - 2) * Math.max(0, Math.min(1, root.level))
            color: Theme.ok
        }
    }
}
