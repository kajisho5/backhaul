import QtQuick
import Backhaul

// One line of a list to pick from.
Rectangle {
    id: root
    property string text: ""
    signal chosen()
    width: parent ? parent.width : 0
    height: 32
    color: area.containsMouse || activeFocus ? Theme.control : "transparent"
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.chosen()
    Keys.onReturnPressed: root.chosen()
    Keys.onSpacePressed: root.chosen()
    Text {
        anchors.verticalCenter: parent.verticalCenter
        x: 10
        width: parent.width - 20
        elide: Text.ElideRight
        text: root.text
        color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13
    }
    MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; onClicked: root.chosen() }
}
