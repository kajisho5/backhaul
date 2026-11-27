import QtQuick
import Backhaul

// Two choices side by side; `first` says whether the left one is chosen.
Rectangle {
    id: root
    property string firstText: ""
    property string secondText: ""
    property bool first: true
    signal chosen(bool first)

    implicitWidth: row.implicitWidth + 2
    implicitHeight: 32
    radius: 2
    color: "transparent"
    border.width: 1
    border.color: Theme.line

    Row {
        id: row
        anchors.centerIn: parent
        PlainButton {
            text: root.firstText
            strong: root.first
            border.width: 0
            onClicked: root.chosen(true)
        }
        PlainButton {
            text: root.secondText
            strong: !root.first
            border.width: 0
            onClicked: root.chosen(false)
        }
    }
}
