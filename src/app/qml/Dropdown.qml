import QtQuick
import Backhaul

// One choice out of a short list. The list opens below the field.
Rectangle {
    id: root
    property var options: []
    property int current: 0
    property bool dimmed: false      // show the current text in the quiet colour
    property string name: ""         // what a screen reader calls this
    readonly property bool open: list.visible
    signal picked(int index)

    implicitHeight: 40
    radius: 2
    color: "#0f1113"
    border.width: 1
    border.color: activeFocus || open ? Theme.ink : Theme.line
    opacity: enabled ? 1.0 : 0.5
    activeFocusOnTab: enabled

    Accessible.role: Accessible.ComboBox
    Accessible.name: root.name + " " + (root.options[root.current] || "")
    Keys.onReturnPressed: list.visible = !list.visible
    Keys.onSpacePressed: list.visible = !list.visible
    Keys.onEscapePressed: list.visible = false
    Keys.onDownPressed: if (root.current + 1 < root.options.length) root.picked(root.current + 1)
    Keys.onUpPressed: if (root.current > 0) root.picked(root.current - 1)
    onActiveFocusChanged: if (!activeFocus) list.visible = false

    Text {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: arrow.left
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        elide: Text.ElideRight
        text: root.options[root.current] || ""
        color: root.dimmed ? Theme.mute : Theme.ink
        font.family: app.sansFamily
        font.pixelSize: 14
    }
    Text {
        id: arrow
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 12
        text: "▼"
        color: Theme.mute
        font.pixelSize: 10
    }
    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.forceActiveFocus()
            list.visible = !list.visible
        }
    }

    Rectangle {
        id: list
        visible: false
        y: root.height + 2
        width: root.width
        height: column.height + 2
        radius: 2
        color: Theme.panel
        border.width: 1
        border.color: Theme.ink
        Column {
            id: column
            x: 1; y: 1
            width: parent.width - 2
            Repeater {
                model: root.options
                delegate: Rectangle {
                    id: option
                    required property int index
                    required property string modelData
                    width: column.width
                    height: 38
                    color: optionArea.containsMouse || index === root.current ? Theme.control : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 12
                        width: parent.width - 24
                        elide: Text.ElideRight
                        text: option.modelData
                        color: Theme.ink
                        font.family: app.sansFamily
                        font.pixelSize: 14
                        font.bold: option.index === root.current
                    }
                    MouseArea {
                        id: optionArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            list.visible = false
                            root.picked(option.index)
                        }
                    }
                }
            }
        }
    }
}
