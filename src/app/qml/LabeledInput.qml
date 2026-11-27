import QtQuick
import QtQuick.Layouts
import Backhaul

// A one-line text field with its label above. With `readOnly` it only shows a value.
ColumnLayout {
    id: root
    property string label: ""
    property string value: ""
    property string placeholder: ""
    property int maxLength: 100
    property bool readOnly: false
    signal edited(string text)
    spacing: 6

    Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: root.label; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 40
        radius: 2
        color: "#0f1113"
        border.width: 1
        border.color: input.activeFocus ? Theme.ink : Theme.line

        TextInput {
            id: input
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            verticalAlignment: TextInput.AlignVCenter
            clip: true
            readOnly: root.readOnly
            activeFocusOnTab: !root.readOnly
            selectByMouse: true
            maximumLength: root.maxLength
            color: Theme.ink
            selectionColor: Theme.ink
            selectedTextColor: Theme.bg
            font.family: app.sansFamily
            font.pixelSize: 14
            text: root.value
            Accessible.name: root.label
            onTextEdited: root.edited(text)
        }
        Text {
            anchors.fill: input
            verticalAlignment: Text.AlignVCenter
            visible: input.text.length === 0 && input.preeditText.length === 0
            text: root.placeholder
            color: Theme.mute
            font: input.font
            elide: Text.ElideRight
        }
    }
}
