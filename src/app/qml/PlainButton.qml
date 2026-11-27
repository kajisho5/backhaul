import QtQuick
import Backhaul

// A flat button. `strong` fills it with the ink colour.
Rectangle {
    id: root
    property string text: ""
    property bool strong: false
    property color tint: Theme.ink
    property int textSize: 12
    signal clicked()

    implicitWidth: label.implicitWidth + 24
    implicitHeight: 30
    radius: 2
    color: strong ? tint : (area.pressed ? Theme.control : "transparent")
    border.width: strong ? 0 : 1
    border.color: activeFocus ? Theme.ink : (tint === Theme.ink ? Theme.line : tint)
    opacity: enabled ? 1.0 : 0.4
    activeFocusOnTab: enabled

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.clicked()
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.strong ? Theme.bg : root.tint
        font.family: app.sansFamily
        font.pixelSize: root.textSize
        font.bold: root.strong
    }
    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
